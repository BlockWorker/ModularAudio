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

        private readonly Timer timer;

        private bool i2c_connected = false;

        public ChargerWindow() {
            InitializeComponent();
            timer = new(OnTimerExpired);
            connPortBox.ItemsSource = SerialPort.GetPortNames();
            connPortBox.Text = App.i2c_portname;
        }

        void OnTimerExpired(object? state) {
            Dispatcher.Invoke(TimerTick);
        }

        private void TimerTick() {
            if (!i2c_connected || App.i2cd.connected == 0) {
                DisconnectReset();
                return;
            }

            var buf = new byte[2];

            if (!I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0xFE, 2, ref buf)) {
                DisconnectReset();
                return;
            }
            mfgIdBox.Text = BitConverter.ToUInt16(buf, 0).ToString("X4");

            if (!I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0xFF, 2, ref buf)) {
                DisconnectReset();
                return;
            }
            devIdBox.Text = BitConverter.ToUInt16(buf, 0).ToString("X4");

            if (!I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0x12, 2, ref buf)) {
                DisconnectReset();
                return;
            }
            optionsLowField.Value = buf[0];
            optionsHighField.Value = buf[1];

            if (!I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0x14, 2, ref buf)) {
                DisconnectReset();
                return;
            }
            if (!chgCurrentBox.IsFocused) {
                chgCurrentBox.Text = BitConverter.ToUInt16(buf, 0).ToString();
            }

            if (!I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0x15, 2, ref buf)) {
                DisconnectReset();
                return;
            }
            if (!chgVoltageBox.IsFocused) {
                chgVoltageBox.Text = BitConverter.ToUInt16(buf, 0).ToString();
            }

            if (!I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0x3F, 2, ref buf)) {
                DisconnectReset();
                return;
            }
            if (!inputCurrentBox.IsFocused) {
                inputCurrentBox.Text = BitConverter.ToUInt16(buf, 0).ToString();
            }
        }

        private void DisconnectReset() {
            i2c_connected = false;
            App.I2CDisconnect(this);

            timer.Change(Timeout.Infinite, Timeout.Infinite);

            connLabel.Content = "Disconnected";
            connPortBox.IsEnabled = true;
            connButton.Content = "Connect";
        }

        private void OnConnectButtonClick(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) {
                string portname = App.i2c_portname;
                if (App.i2cd.connected == 0) {
                    if (connPortBox.SelectedIndex < 0) {
                        MessageBox.Show("Please select a port!");
                        return;
                    }
                    portname = (string)connPortBox.SelectedValue;
                }

                if (!App.I2CConnect(this, portname)) {
                    MessageBox.Show("Connection failed...");
                    DisconnectReset();
                    return;
                }

                i2c_connected = true;
                connLabel.Content = "Connected";
                connPortBox.IsEnabled = false;
                connButton.Content = "Disconnect";

                timer.Change(0, 1000);
            } else {
                DisconnectReset();
            }
        }

        private void DoOptionsApply(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            var buf = new byte[2] { optionsLowField.SwitchedValue, optionsHighField.SwitchedValue };

            if (!I2CDriverLib.I2C_WriteReg(ref App.i2cd, DEV_ADDR, 0x12, buf)) {
                DisconnectReset();
                return;
            }

            optionsLowField.ResetSwitches();
            optionsHighField.ResetSwitches();

            if (I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0x12, 2, ref buf)) {
                optionsLowField.Value = buf[0];
                optionsHighField.Value = buf[1];
            }
        }

        private void DoChargeCurrentApply(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            if (!ushort.TryParse(chgCurrentBox.Text, out var cur)) return;

            var buf = new byte[2];

            if (!I2CDriverLib.I2C_WriteReg(ref App.i2cd, DEV_ADDR, 0x14, BitConverter.GetBytes((ushort)(cur & 0x1FC0)))) {
                DisconnectReset();
                return;
            }

            if (I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0x14, 2, ref buf)) {
                chgCurrentBox.Text = BitConverter.ToUInt16(buf, 0).ToString();
            }
        }

        private void DoChargeVoltageApply(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            if (!ushort.TryParse(chgVoltageBox.Text, out var voltage)) return;

            var buf = new byte[2];

            if (!I2CDriverLib.I2C_WriteReg(ref App.i2cd, DEV_ADDR, 0x15, BitConverter.GetBytes((ushort)(voltage & 0x7FF0)))) {
                DisconnectReset();
                return;
            }

            if (I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0x15, 2, ref buf)) {
                chgVoltageBox.Text = BitConverter.ToUInt16(buf, 0).ToString();
            }
        }

        private void DoInputCurrentApply(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            if (!ushort.TryParse(inputCurrentBox.Text, out var cur)) return;

            var buf = new byte[2];

            if (!I2CDriverLib.I2C_WriteReg(ref App.i2cd, DEV_ADDR, 0x3F, BitConverter.GetBytes((ushort)(cur & 0x1F80)))) {
                DisconnectReset();
                return;
            }

            if (I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0x3F, 2, ref buf)) {
                inputCurrentBox.Text = BitConverter.ToUInt16(buf, 0).ToString();
            }
        }

        private void OnWindowClosing(object sender, System.ComponentModel.CancelEventArgs e) {
            DisconnectReset();
        }
    }
}
