"""Motor Controller Tuner GUI.

Ported unchanged from legacy Motorcontroller_v2.0gui's own tuner script -- the roboteq
wire protocol it speaks (?ALLVARS, !VAR <n> <value>, ?TRN, ^ECHOF) is identical to what
InterfaceMotorcontroller.cpp implements today, so this needed no protocol changes.

Note: "Error Fraction" (param 7) and "Error Counts" (param 8, allowedError) are still
readable/settable through this GUI for wire-protocol compatibility, but no longer have
any real safety effect on the firmware side -- they belonged to the legacy sketch's own
encoder-vs-PWM-delta watchdog, which has been replaced entirely by
meijworks::SteeringActuator's own overcurrent/overpressure/interlock safety system (see
Motorcontroller/test/README's "Known gaps").
"""
import tkinter as tk
from tkinter import messagebox
from tkinter import ttk
import serial
import serial.tools.list_ports
import sys
import os

# Function to send command to the device
def send_command(command):
    try:
        ser.write((command + '\r\n').encode())
        response = ser.readline().decode().strip()
        return response
    except Exception as e:
        messagebox.showerror("Error", f"Failed to send command: {e}")
        return None

# Function to get all variable values at once
def get_all_variables():
    response = send_command('?ALLVARS')
    if response:
        if response.startswith('VARS='):
            values = response[5:].split(',')
            if len(values) == len(variables):
                for i, var_info in enumerate(variables):
                    current_value = values[i]
                    var_info["current_var"].set(current_value)
                    var_info["var"].set(current_value)
            else:
                messagebox.showerror("Error", "Invalid response from device.")
        else:
            messagebox.showerror("Error", "Invalid response from device.")
    else:
        messagebox.showerror("Error", "No response from device.")

# Function to update variables with validation
def update_variables():
    if not ser.is_open:
        messagebox.showerror("Error", "Serial port is not connected.")
        return

    errors = []
    commands = []

    for var_info in variables:
        name = var_info['name']
        min_value = var_info['min']
        max_value = var_info['max']
        user_input = var_info['var'].get().strip()

        if not user_input:
            # If the input is empty, skip updating this variable
            continue

        try:
            value = int(user_input)
        except ValueError:
            errors.append(f"{name} must be an integer.")
            continue

        if value < min_value or value > max_value:
            errors.append(f"{name} must be between {min_value} and {max_value}.")
        else:
            param_num = var_info['param_num']
            commands.append(f'!VAR {param_num} {value}')

    if errors:
        messagebox.showerror("Input Error", "\n".join(errors))
        return

    # Send commands to update variables
    for command in commands:
        send_command(command)

    get_all_variables()  # Refresh current values after update
    messagebox.showinfo("Success", "Variables updated successfully.")

# Function to open the tuning information window
def open_tuning_info():
    tuning_window = tk.Toplevel(root)
    tuning_window.title("Tuning Information")
    tuning_window.geometry("600x500")
    tuning_window.resizable(False, False)
    try:
        icon_path1 = resource_path('favicon.ico')
        tuning_window.iconbitmap(icon_path1)
    except:
        pass  # If the icon file is not found, ignore

    description_text = (
        "Tuning Information:\n\n"
        "KP (Proportional Gain):\n"
        "- Increases the system's responsiveness.\n"
        "- Too high can cause overshoot and instability.\n\n"
        "KI (Integral Gain):\n"
        "- Eliminates steady-state error.\n"
        "- Too high can cause slow response and oscillations.\n\n"
        "KD (Derivative Gain):\n"
        "- Reduces overshoot and oscillations.\n"
        "- Too high can cause noise amplification and instability.\n\n"
        "Min Power:\n"
        "- Minimum power given to the motor controller.\n\n"
        "Error Fraction / Error Counts:\n"
        "- No longer have any real effect on the firmware's safety behavior --\n"
        "  readable/settable here for wire-protocol compatibility only.\n"
    )
    description_label = ttk.Label(tuning_window, text=description_text, wraplength=580, justify='left')
    description_label.pack(pady=10, padx=10)

# Function to automatically scan and connect to the correct COM port
def auto_connect():
    ports = serial.tools.list_ports.comports()
    print(ports)
    found = False
    for port in ports:
        try:
            temp_ser = serial.Serial(port.device, 115200, timeout=1)
            temp_ser.reset_input_buffer()
            send_command('^ECHOF 1\r\n')
            temp_ser.write(b'?TRN\r\n')
            response = temp_ser.readline().decode().strip()
            if response.startswith('TRN=SDC2XXX:SDC2160S'):
                global ser
                ser = temp_ser
                messagebox.showinfo("Success", f"Automatically connected to {port.device}")
                auto_connect_button.config(state=tk.DISABLED)
                disconnect_button.config(state=tk.NORMAL)
                update_button.config(state=tk.NORMAL)
                get_all_variables()
                found = True
                break
            else:
                temp_ser.close()
        except Exception:
            continue  # Try next port

    if not found:
        messagebox.showerror("Connection Error", "Failed to find the controller. Make sure it is plugged in.")



def disconnect_serial():
    global ser
    try:
        if ser.is_open:
            ser.close()
            messagebox.showinfo("Disconnected", "Serial port disconnected.")
            auto_connect_button.config(state=tk.NORMAL)
            disconnect_button.config(state=tk.DISABLED)
            update_button.config(state=tk.DISABLED)
    except Exception as e:
        messagebox.showerror("Error", f"Failed to disconnect: {e}")

# Function to get the resource path
def resource_path(relative_path):
    """ Get absolute path to resource, works for dev and PyInstaller """
    try:
        # PyInstaller creates a temp folder and stores path in _MEIPASS
        base_path = sys._MEIPASS
    except Exception:
        base_path = os.path.abspath(".")
    return os.path.join(base_path, relative_path)

# Create main window
root = tk.Tk()
root.title("Motor Controller Tuner")
root.geometry("600x500")
root.resizable(False, False)

# Apply style
style = ttk.Style(root)
style.theme_use('clam')

# Set the window icon
try:
    icon_path = resource_path('icon.ico')
    root.iconbitmap(icon_path)
except:
    pass  # If the icon file is not found, ignore

# Header Frame
header_frame = ttk.Frame(root)
header_frame.pack(pady=10)

header_label = ttk.Label(header_frame, text="Motor Controller Tuner", font=("Helvetica", 18, 'bold'))
header_label.pack()

# Serial port connection
port_frame = ttk.Frame(root)
port_frame.pack(pady=10, fill='x')

header_label = ttk.Label(port_frame, text="Connect comport:", font=("Helvetica", 12, 'bold'))
header_label.pack(side=tk.LEFT, padx=5)

auto_connect_button = ttk.Button(port_frame, text="Connect", command=auto_connect)
auto_connect_button.pack(side=tk.LEFT, padx=5)


disconnect_button = ttk.Button(port_frame, text="Disconnect", command=disconnect_serial)
disconnect_button.pack(side=tk.LEFT, padx=5)
disconnect_button.config(state=tk.DISABLED)


# Separator
separator = ttk.Separator(root, orient='horizontal')
separator.pack(fill='x', pady=5)

# Variables Frame
vars_frame = ttk.Frame(root)
vars_frame.pack(pady=10, fill='x')

# Column headings
header_labels = ["Variable", "Current Value", "New Value", "Default Value"]
for col, text in enumerate(header_labels):
    header = ttk.Label(vars_frame, text=text, font=('Helvetica', 10, 'bold'))
    header.grid(row=0, column=col, padx=5, pady=5, sticky='nsew')

vars_frame.columnconfigure(0, weight=1)
vars_frame.columnconfigure(1, weight=1)
vars_frame.columnconfigure(2, weight=1)
vars_frame.columnconfigure(3, weight=1)

# Default Values and Variable Definitions -- param_num/min/max/default match
# InterfaceMotorcontroller.cpp's own kKpDefault/kKiDefault/.../kAllowedErrorDefault and
# ReadConfig()'s validated ranges.
variables = [
    {"name": "KP (Proportional)", "default_value": 3, "min": 1, "max": 10, "param_num": 1, "var": None, "current_var": None, "entry": None},
    {"name": "KI (Integral)", "default_value": 2, "min": 0, "max": 10, "param_num": 2, "var": None, "current_var": None, "entry": None},
    {"name": "KD (Derivative)", "default_value": 0, "min": 0, "max": 10, "param_num": 3, "var": None, "current_var": None, "entry": None},
    {"name": "Min Power", "default_value": 50, "min": 20, "max": 100, "param_num": 5, "var": None, "current_var": None, "entry": None},
    {"name": "Max Power", "default_value": 190, "min": 100, "max": 250, "param_num": 6, "var": None, "current_var": None, "entry": None},
    {"name": "Error Fraction (vestigial, no real effect)", "default_value": 15, "min": 5, "max": 50, "param_num": 7, "var": None, "current_var": None, "entry": None},
    {"name": "Error Counts (vestigial, no real effect)", "default_value": 500000, "min": 400000, "max": 600000, "param_num": 8, "var": None, "current_var": None, "entry": None}
]

# Create rows for each variable
for idx, var_info in enumerate(variables, start=1):
    # Variable Name
    name_label = ttk.Label(vars_frame, text=var_info["name"])
    name_label.grid(row=idx, column=0, padx=5, pady=5, sticky='w')

    # Current Value
    var_info["current_var"] = tk.StringVar(value="")
    current_label = ttk.Label(vars_frame, textvariable=var_info["current_var"])
    current_label.grid(row=idx, column=1, padx=5, pady=5)

    # New Value Entry
    var_info["var"] = tk.StringVar()
    entry = ttk.Entry(vars_frame, textvariable=var_info["var"], width=10)
    entry.grid(row=idx, column=2, padx=5, pady=5)
    var_info["entry"] = entry

    # Default Value
    default_str = f"{var_info['default_value']} (min {var_info['min']} max {var_info['max']})"
    default_label = ttk.Label(vars_frame, text=default_str, width=25)
    default_label.grid(row=idx, column=3, padx=5, pady=5)



# Update Button
update_button = ttk.Button(root, text="Update Values", command=update_variables)
update_button.pack(pady=5)
update_button.config(state=tk.DISABLED)
# Button to open Tuning Information window
tuning_button = ttk.Button(root, text="Tuning Information", command=open_tuning_info)
tuning_button.pack(pady=5)

# Initialize serial connection variable
ser = serial.Serial()

# Start the GUI event loop
root.mainloop()
