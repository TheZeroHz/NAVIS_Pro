import pygame
import serial
import serial.tools.list_ports
import json
import sys
import threading
import queue
from collections import deque
import time


class CursorVisualizer:
    def __init__(self, serial_port=None, baud_rate=115200, width=1200, height=800):
        # Initialize Pygame
        pygame.init()

        # Screen setup
        self.width = width
        self.height = height
        self.screen = pygame.display.set_mode((width, height))
        pygame.display.set_caption("BNO055 Hand Mouse Visualizer")

        # Colors
        self.BLACK = (0, 0, 0)
        self.WHITE = (255, 255, 255)
        self.RED = (255, 0, 0)
        self.GREEN = (0, 255, 0)
        self.BLUE = (0, 0, 255)
        self.GRAY = (128, 128, 128)
        self.LIGHT_GRAY = (200, 200, 200)
        self.YELLOW = (255, 255, 0)

        # Cursor state
        self.cursor_x = width // 2
        self.cursor_y = height // 2
        self.button_pressed = False

        # Trail for showing cursor path
        self.trail = deque(maxlen=100)  # Keep last 100 positions

        # Serial communication
        self.serial_port = serial_port
        self.baud_rate = baud_rate
        self.serial_conn = None
        self.data_queue = queue.Queue()
        self.running = True

        # Statistics
        self.total_movements = 0
        self.button_presses = 0
        self.start_time = time.time()

        # Font for text display
        self.font = pygame.font.Font(None, 24)
        self.small_font = pygame.font.Font(None, 18)

        # Connect to serial
        self.connect_serial()

        # Start serial reading thread
        self.serial_thread = threading.Thread(target=self.read_serial, daemon=True)
        self.serial_thread.start()

    def connect_serial(self):
        """Connect to Arduino via serial port with user selection"""
        # If no port specified, show selection menu
        if not self.serial_port:
            self.serial_port = self.select_com_port()
            if not self.serial_port:
                print("No port selected. Exiting...")
                sys.exit(1)

        try:
            self.serial_conn = serial.Serial(self.serial_port, self.baud_rate, timeout=0.1)
            print(f"✓ Connected to {self.serial_port} at {self.baud_rate} baud")
            time.sleep(2)  # Wait for Arduino to initialize
        except serial.SerialException as e:
            print(f"✗ Failed to connect to {self.serial_port}: {e}")
            # Show port selection again if connection fails
            print("\nTrying port selection again...")
            self.serial_port = self.select_com_port()
            if self.serial_port:
                self.connect_serial()  # Recursive retry
            else:
                sys.exit(1)

    def select_com_port(self):
        """Interactive COM port selection"""
        print("\n" + "=" * 50)
        print("BNO055 Cursor Visualizer - COM Port Selection")
        print("=" * 50)

        # Get available ports
        ports = list(serial.tools.list_ports.comports())

        if not ports:
            print("❌ No COM ports found!")
            print("Make sure your Arduino is connected and drivers are installed.")
            return None

        print(f"\nFound {len(ports)} available port(s):\n")

        # Display ports with detailed info
        for i, (port, desc, hwid) in enumerate(ports):
            # Try to identify Arduino-like devices
            is_arduino = any(keyword in desc.lower() for keyword in
                             ['arduino', 'ch340', 'cp210', 'ftdi', 'usb-serial'])

            marker = "🔶 [LIKELY ARDUINO]" if is_arduino else "⚪"

            print(f"{i + 1:2d}. {marker} {port}")
            print(f"     Description: {desc}")
            print(f"     Hardware ID: {hwid}")
            print()

        while True:
            try:
                print("Options:")
                print("  • Enter number (1-{}) to select port".format(len(ports)))
                print("  • Enter 'r' to refresh port list")
                print("  • Enter 'q' to quit")

                choice = input(f"\nSelect port (1-{len(ports)}) or option: ").strip().lower()

                if choice == 'q':
                    return None
                elif choice == 'r':
                    return self.select_com_port()  # Refresh and show menu again
                elif choice.isdigit():
                    port_index = int(choice) - 1
                    if 0 <= port_index < len(ports):
                        selected_port = ports[port_index][0]

                        # Test connection
                        print(f"\n🔄 Testing connection to {selected_port}...")
                        if self.test_port_connection(selected_port):
                            print(f"✓ Port {selected_port} is working!")
                            return selected_port
                        else:
                            print(f"✗ Could not connect to {selected_port}")
                            print("Try another port or check your Arduino connection.")
                            continue
                    else:
                        print(f"❌ Invalid selection. Please enter 1-{len(ports)}")
                else:
                    print("❌ Invalid input. Please try again.")

            except (ValueError, KeyboardInterrupt):
                print("\n👋 Goodbye!")
                return None
            except Exception as e:
                print(f"❌ Error: {e}")
                continue

    def test_port_connection(self, port):
        """Test if we can connect to the specified port"""
        try:
            test_conn = serial.Serial(port, self.baud_rate, timeout=1)
            time.sleep(1)  # Give Arduino time to initialize

            # Try to read some data
            start_time = time.time()
            data_received = False

            while time.time() - start_time < 3:  # Wait up to 3 seconds
                if test_conn.in_waiting:
                    try:
                        line = test_conn.readline().decode('utf-8').strip()
                        if line:  # Any data received
                            data_received = True
                            break
                    except:
                        pass
                time.sleep(0.1)

            test_conn.close()
            return data_received

        except Exception:
            return False

    def read_serial(self):
        """Read data from serial port in separate thread"""
        while self.running:
            try:
                if self.serial_conn and self.serial_conn.in_waiting:
                    line = self.serial_conn.readline().decode('utf-8').strip()
                    if line:
                        self.data_queue.put(line)
            except Exception as e:
                print(f"Serial read error: {e}")
            time.sleep(0.001)  # Small delay to prevent CPU hogging

    def process_serial_data(self):
        """Process incoming serial data"""
        while not self.data_queue.empty():
            try:
                line = self.data_queue.get_nowait()

                # Try to parse JSON data
                if line.startswith('{'):
                    data = json.loads(line)

                    if 'mouse' in data:
                        mouse_data = data['mouse']
                        dx = mouse_data.get('dx', 0)
                        dy = mouse_data.get('dy', 0)
                        button = mouse_data.get('button', False)

                        # Update cursor position
                        if button and (abs(dx) > 0 or abs(dy) > 0):
                            self.cursor_x += dx
                            self.cursor_y += dy

                            # Keep cursor within screen bounds
                            self.cursor_x = max(10, min(self.width - 10, self.cursor_x))
                            self.cursor_y = max(10, min(self.height - 10, self.cursor_y))

                            # Add to trail if button is pressed
                            if button:
                                self.trail.append((self.cursor_x, self.cursor_y))

                            self.total_movements += 1

                        self.button_pressed = button

                    elif 'event' in data:
                        event = data['event']
                        if event == 'button_press':
                            self.button_presses += 1
                            self.trail.clear()  # Clear trail on new press
                        print(f"Event: {event}")

                    elif 'status' in data:
                        print(f"Status: {data['status']}")

            except json.JSONDecodeError:
                # Not JSON data, just print it
                print(f"Arduino: {line}")
            except Exception as e:
                print(f"Data processing error: {e}")

    def draw_grid(self):
        """Draw background grid"""
        grid_size = 50
        for x in range(0, self.width, grid_size):
            pygame.draw.line(self.screen, self.LIGHT_GRAY, (x, 0), (x, self.height), 1)
        for y in range(0, self.height, grid_size):
            pygame.draw.line(self.screen, self.LIGHT_GRAY, (0, y), (self.width, y), 1)

    def draw_trail(self):
        """Draw cursor movement trail"""
        if len(self.trail) > 1:
            for i in range(1, len(self.trail)):
                alpha = int(255 * (i / len(self.trail)))  # Fade effect
                color = (0, alpha, 255)  # Blue trail

                start_pos = self.trail[i - 1]
                end_pos = self.trail[i]

                # Draw line with varying thickness
                thickness = max(1, int(3 * (i / len(self.trail))))
                pygame.draw.line(self.screen, color, start_pos, end_pos, thickness)

    def draw_cursor(self):
        """Draw the cursor"""
        cursor_size = 15 if self.button_pressed else 10
        cursor_color = self.RED if self.button_pressed else self.BLUE

        # Draw cursor circle
        pygame.draw.circle(self.screen, cursor_color,
                           (int(self.cursor_x), int(self.cursor_y)), cursor_size)

        # Draw cursor crosshair
        pygame.draw.line(self.screen, self.WHITE,
                         (self.cursor_x - cursor_size, self.cursor_y),
                         (self.cursor_x + cursor_size, self.cursor_y), 2)
        pygame.draw.line(self.screen, self.WHITE,
                         (self.cursor_x, self.cursor_y - cursor_size),
                         (self.cursor_x, self.cursor_y + cursor_size), 2)

    def draw_info_panel(self):
        """Draw information panel"""
        panel_height = 120
        pygame.draw.rect(self.screen, (40, 40, 40), (10, 10, 300, panel_height))
        pygame.draw.rect(self.screen, self.WHITE, (10, 10, 300, panel_height), 2)

        # Status text
        y_offset = 25

        # Connection status
        status_color = self.GREEN if self.serial_conn and self.serial_conn.is_open else self.RED
        status_text = "Connected" if self.serial_conn and self.serial_conn.is_open else "Disconnected"
        text = self.font.render(f"Serial: {status_text}", True, status_color)
        self.screen.blit(text, (20, y_offset))
        y_offset += 25

        # Button status
        button_color = self.RED if self.button_pressed else self.GRAY
        button_text = "PRESSED" if self.button_pressed else "Released"
        text = self.font.render(f"Button: {button_text}", True, button_color)
        self.screen.blit(text, (20, y_offset))
        y_offset += 20

        # Cursor position
        text = self.small_font.render(f"Position: ({int(self.cursor_x)}, {int(self.cursor_y)})", True, self.WHITE)
        self.screen.blit(text, (20, y_offset))
        y_offset += 18

        # Statistics
        runtime = time.time() - self.start_time
        text = self.small_font.render(f"Movements: {self.total_movements}", True, self.WHITE)
        self.screen.blit(text, (20, y_offset))
        y_offset += 18

        text = self.small_font.render(f"Button presses: {self.button_presses}", True, self.WHITE)
        self.screen.blit(text, (20, y_offset))

    def draw_instructions(self):
        """Draw usage instructions"""
        instructions = [
            "Instructions:",
            "• Hold Arduino button to move cursor",
            "• Rotate device to control movement",
            "• Red cursor = button pressed",
            "• Blue trail shows movement path",
            "• Press ESC or Q to quit",
            "• Press R to reset cursor position",
            "• Press C to clear trail"
        ]

        panel_width = 350
        panel_height = len(instructions) * 18 + 20
        start_x = self.width - panel_width - 10
        start_y = 10

        pygame.draw.rect(self.screen, (40, 40, 40), (start_x, start_y, panel_width, panel_height))
        pygame.draw.rect(self.screen, self.WHITE, (start_x, start_y, panel_width, panel_height), 2)

        for i, instruction in enumerate(instructions):
            color = self.YELLOW if i == 0 else self.WHITE
            font = self.font if i == 0 else self.small_font
            text = font.render(instruction, True, color)
            self.screen.blit(text, (start_x + 10, start_y + 10 + i * 18))

    def handle_keyboard(self, event):
        """Handle keyboard events"""
        if event.type == pygame.KEYDOWN:
            if event.key == pygame.K_r:
                # Reset cursor position
                self.cursor_x = self.width // 2
                self.cursor_y = self.height // 2
                self.trail.clear()
                print("Cursor position reset")

            elif event.key == pygame.K_c:
                # Clear trail
                self.trail.clear()
                print("Trail cleared")

            elif event.key in [pygame.K_q, pygame.K_ESCAPE]:
                self.running = False

    def run(self):
        """Main visualization loop"""
        clock = pygame.time.Clock()

        print("BNO055 Cursor Visualizer Started")
        print("Hold the button on your Arduino and move the device to control the cursor!")

        while self.running:
            # Handle events
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    self.running = False
                else:
                    self.handle_keyboard(event)

            # Process serial data
            self.process_serial_data()

            # Clear screen
            self.screen.fill(self.BLACK)

            # Draw elements
            self.draw_grid()
            self.draw_trail()
            self.draw_cursor()
            self.draw_info_panel()
            self.draw_instructions()

            # Update display
            pygame.display.flip()
            clock.tick(60)  # 60 FPS

        # Cleanup
        if self.serial_conn:
            self.serial_conn.close()
        pygame.quit()
        print("Visualizer closed")


if __name__ == "__main__":
    # Configuration
    SERIAL_PORT = None  # Will show selection menu if None
    BAUD_RATE = 115200

    # You can also pass port as command line argument
    if len(sys.argv) > 1:
        SERIAL_PORT = sys.argv[1]
        print(f"Using port from command line: {SERIAL_PORT}")

    try:
        visualizer = CursorVisualizer(SERIAL_PORT, BAUD_RATE)
        visualizer.run()
    except KeyboardInterrupt:
        print("\n👋 Goodbye!")
    except Exception as e:
        print(f"💥 Error: {e}")
        input("Press Enter to exit...")
        sys.exit(1)
