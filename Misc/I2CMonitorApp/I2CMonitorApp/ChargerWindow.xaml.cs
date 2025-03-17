using System;
using System.Collections.Generic;
using System.IO.Ports;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;

namespace I2CMonitorApp {
    /// <summary>
    /// Interaction logic for ChargerWindow.xaml
    /// </summary>
    public partial class ChargerWindow : Window {

        private const byte DEV_ADDR = 0x09;
        private const byte DEVICE_ID_CORRECT = 0xA1;

        private readonly Timer timer;

        private I2CDriverLib.I2CDriver d;

        public ChargerWindow() {
            InitializeComponent();
            timer = new(OnTimerExpired);
            d = new();
            connPortBox.ItemsSource = SerialPort.GetPortNames();
        }

        void OnTimerExpired(object? state) {
            Dispatcher.Invoke(TimerTick);
        }

        private void TimerTick() {
            if (d.connected == 0) {
                timer.Change(Timeout.Infinite, Timeout.Infinite);
                return;
            }

            var buf = new byte[2];

            if (!I2CDriverLib.I2C_ReadReg(ref d, DEV_ADDR, 0xFE, 2, ref buf)) {
                DisconnectReset();
                return;
            }
            mfgIdBox.Text = BitConverter.ToUInt16(buf, 0).ToString("X4");

            if (!I2CDriverLib.I2C_ReadReg(ref d, DEV_ADDR, 0xFF, 2, ref buf)) {
                DisconnectReset();
                return;
            }
            devIdBox.Text = BitConverter.ToUInt16(buf, 0).ToString("X4");

            if (!I2CDriverLib.I2C_ReadReg(ref d, DEV_ADDR, 0x12, 2, ref buf)) {
                DisconnectReset();
                return;
            }
            optionsLowField.Value = buf[0];
            optionsHighField.Value = buf[1];

            if (!I2CDriverLib.I2C_ReadReg(ref d, DEV_ADDR, 0x14, 2, ref buf)) {
                DisconnectReset();
                return;
            }
            if (!chgCurrentBox.IsFocused) {
                chgCurrentBox.Text = BitConverter.ToUInt16(buf, 0).ToString();
            }

            if (!I2CDriverLib.I2C_ReadReg(ref d, DEV_ADDR, 0x15, 2, ref buf)) {
                DisconnectReset();
                return;
            }
            if (!chgVoltageBox.IsFocused) {
                chgVoltageBox.Text = BitConverter.ToUInt16(buf, 0).ToString();
            }

            if (!I2CDriverLib.I2C_ReadReg(ref d, DEV_ADDR, 0x3F, 2, ref buf)) {
                DisconnectReset();
                return;
            }
            if (!inputCurrentBox.IsFocused) {
                inputCurrentBox.Text = BitConverter.ToUInt16(buf, 0).ToString();
            }
        }

        private void DisconnectReset() {
            I2CDriverLib.i2c_disconnect(ref d);

            timer.Change(Timeout.Infinite, Timeout.Infinite);

            connLabel.Content = "Disconnected";
            idLabel.Content = "0x00";
            connPortBox.IsEnabled = true;
            connButton.Content = "Connect";
        }

        private void OnConnectButtonClick(object sender, RoutedEventArgs e) {
            if (d.connected == 0) {
                if (connPortBox.SelectedIndex < 0) {
                    MessageBox.Show("Please select a port!");
                    return;
                }

                I2CDriverLib.i2c_connect(ref d, (string)connPortBox.SelectedValue);

                if (d.connected == 0) {
                    MessageBox.Show("Connection failed...");
                    return;
                }

                connLabel.Content = "Connected";
                connPortBox.IsEnabled = false;
                connButton.Content = "Disconnect";

                timer.Change(0, 1000);
            } else {
                I2CDriverLib.i2c_disconnect(ref d);

                timer.Change(Timeout.Infinite, Timeout.Infinite);

                connLabel.Content = "Disconnected";
                idLabel.Content = "0x00";
                connPortBox.IsEnabled = true;
                connButton.Content = "Connect";
            }
        }

        private void DoOptionsApply(object sender, RoutedEventArgs e) {
            if (d.connected == 0) return;

            var buf = new byte[2] { optionsLowField.SwitchedValue, optionsHighField.SwitchedValue };

            if (!I2CDriverLib.I2C_WriteReg(ref d, DEV_ADDR, 0x12, buf)) {
                DisconnectReset();
                return;
            }

            optionsLowField.ResetSwitches();
            optionsHighField.ResetSwitches();

            if (I2CDriverLib.I2C_ReadReg(ref d, DEV_ADDR, 0x12, 2, ref buf)) {
                optionsLowField.Value = buf[0];
                optionsHighField.Value = buf[1];
            }
        }

        private void DoChargeCurrentApply(object sender, RoutedEventArgs e) {
            if (d.connected == 0) return;

            if (!ushort.TryParse(chgCurrentBox.Text, out var cur)) return;

            var buf = new byte[2];

            if (!I2CDriverLib.I2C_WriteReg(ref d, DEV_ADDR, 0x14, BitConverter.GetBytes((ushort)(cur & 0x1FC0)))) {
                DisconnectReset();
                return;
            }

            if (I2CDriverLib.I2C_ReadReg(ref d, DEV_ADDR, 0x14, 2, ref buf)) {
                chgCurrentBox.Text = BitConverter.ToUInt16(buf, 0).ToString();
            }
        }

        private void DoChargeVoltageApply(object sender, RoutedEventArgs e) {
            if (d.connected == 0) return;

            if (!ushort.TryParse(chgVoltageBox.Text, out var voltage)) return;

            var buf = new byte[2];

            if (!I2CDriverLib.I2C_WriteReg(ref d, DEV_ADDR, 0x15, BitConverter.GetBytes((ushort)(voltage & 0x7FF0)))) {
                DisconnectReset();
                return;
            }

            if (I2CDriverLib.I2C_ReadReg(ref d, DEV_ADDR, 0x15, 2, ref buf)) {
                chgVoltageBox.Text = BitConverter.ToUInt16(buf, 0).ToString();
            }
        }

        private void DoInputCurrentApply(object sender, RoutedEventArgs e) {
            if (d.connected == 0) return;

            if (!ushort.TryParse(inputCurrentBox.Text, out var cur)) return;

            var buf = new byte[2];

            if (!I2CDriverLib.I2C_WriteReg(ref d, DEV_ADDR, 0x3F, BitConverter.GetBytes((ushort)(cur & 0x1F80)))) {
                DisconnectReset();
                return;
            }

            if (I2CDriverLib.I2C_ReadReg(ref d, DEV_ADDR, 0x3F, 2, ref buf)) {
                inputCurrentBox.Text = BitConverter.ToUInt16(buf, 0).ToString();
            }
        }

        private void OnWindowClosing(object sender, System.ComponentModel.CancelEventArgs e) {
            DisconnectReset();
        }
    }
}
