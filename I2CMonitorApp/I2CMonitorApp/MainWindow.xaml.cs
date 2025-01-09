using System.IO.Ports;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace I2CMonitorApp {
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window {

        private const byte DEV_ADDR = 0x11;
        private const int READ_ALL_PERIOD = 5;
        private const byte DEVICE_ID_CORRECT = 0xA1;

        private readonly Timer timer;
        private int readAllCycles;

        private I2CDriverLib.I2CDriver d;

        public MainWindow() {
            InitializeComponent();
            timer = new(OnTimerExpired);
            readAllCycles = READ_ALL_PERIOD;
            d = new();
            connPortBox.ItemsSource = SerialPort.GetPortNames();
        }

        void OnTimerExpired(object? state) {
            Dispatcher.Invoke(TimerTick);
        }

        private void TimerTick() {
            if (d.connected == 0) {
                timer.Change(Timeout.Infinite, Timeout.Infinite);
                readAllCycles = READ_ALL_PERIOD;
                return;
            }

            var buf = new byte[256];

            if (!I2CDriverLib.I2C_ReadReg(ref d, DEV_ADDR, 0xFF, 1, ref buf)) {
                DisconnectReset();
                return;
            }
            if (buf[0] != DEVICE_ID_CORRECT) {
                DisconnectReset();
                return;
            }

            if (!I2CDriverLib.I2C_ReadReg(ref d, DEV_ADDR, 0x01, 2, ref buf)) {
                DisconnectReset();
                return;
            }
            statusLowField.Value = buf[0];
            statusHighField.Value = buf[1];

            if (!I2CDriverLib.I2C_ReadReg(ref d, DEV_ADDR, 0x11, 1, ref buf)) {
                DisconnectReset();
                return;
            }
            intFlagsField.Value = buf[0];

            if (!I2CDriverLib.I2C_ReadReg(ref d, DEV_ADDR, 0x21, 4, ref buf)) {
                DisconnectReset();
                return;
            }
            pvddReqBox.Text = BitConverter.ToSingle(buf, 0).ToString("F2");

            if (!I2CDriverLib.I2C_ReadReg(ref d, DEV_ADDR, 0x22, 4, ref buf)) {
                DisconnectReset();
                return;
            }
            pvddMeasBox.Text = BitConverter.ToSingle(buf, 0).ToString("F2");

            if (!I2CDriverLib.I2C_ReadReg(ref d, DEV_ADDR, 0x30, 128, ref buf)) {
                DisconnectReset();
                return;
            }
            vAFastBox.Text = BitConverter.ToSingle(buf, 0x00).ToString("F3");
            vBFastBox.Text = BitConverter.ToSingle(buf, 0x04).ToString("F3");
            vCFastBox.Text = BitConverter.ToSingle(buf, 0x08).ToString("F3");
            vDFastBox.Text = BitConverter.ToSingle(buf, 0x0C).ToString("F3");
            iAFastBox.Text = BitConverter.ToSingle(buf, 0x10).ToString("F3");
            iBFastBox.Text = BitConverter.ToSingle(buf, 0x14).ToString("F3");
            iCFastBox.Text = BitConverter.ToSingle(buf, 0x18).ToString("F3");
            iDFastBox.Text = BitConverter.ToSingle(buf, 0x1C).ToString("F3");
            pAFastBox.Text = BitConverter.ToSingle(buf, 0x20).ToString("F3");
            pBFastBox.Text = BitConverter.ToSingle(buf, 0x24).ToString("F3");
            pCFastBox.Text = BitConverter.ToSingle(buf, 0x28).ToString("F3");
            pDFastBox.Text = BitConverter.ToSingle(buf, 0x2C).ToString("F3");
            paAFastBox.Text = BitConverter.ToSingle(buf, 0x30).ToString("F3");
            paBFastBox.Text = BitConverter.ToSingle(buf, 0x34).ToString("F3");
            paCFastBox.Text = BitConverter.ToSingle(buf, 0x38).ToString("F3");
            paDFastBox.Text = BitConverter.ToSingle(buf, 0x3C).ToString("F3");
            vASlowBox.Text = BitConverter.ToSingle(buf, 0x40).ToString("F3");
            vBSlowBox.Text = BitConverter.ToSingle(buf, 0x44).ToString("F3");
            vCSlowBox.Text = BitConverter.ToSingle(buf, 0x48).ToString("F3");
            vDSlowBox.Text = BitConverter.ToSingle(buf, 0x4C).ToString("F3");
            iASlowBox.Text = BitConverter.ToSingle(buf, 0x50).ToString("F3");
            iBSlowBox.Text = BitConverter.ToSingle(buf, 0x54).ToString("F3");
            iCSlowBox.Text = BitConverter.ToSingle(buf, 0x58).ToString("F3");
            iDSlowBox.Text = BitConverter.ToSingle(buf, 0x5C).ToString("F3");
            pASlowBox.Text = BitConverter.ToSingle(buf, 0x60).ToString("F3");
            pBSlowBox.Text = BitConverter.ToSingle(buf, 0x64).ToString("F3");
            pCSlowBox.Text = BitConverter.ToSingle(buf, 0x68).ToString("F3");
            pDSlowBox.Text = BitConverter.ToSingle(buf, 0x6C).ToString("F3");
            paASlowBox.Text = BitConverter.ToSingle(buf, 0x70).ToString("F3");
            paBSlowBox.Text = BitConverter.ToSingle(buf, 0x74).ToString("F3");
            paCSlowBox.Text = BitConverter.ToSingle(buf, 0x78).ToString("F3");
            paDSlowBox.Text = BitConverter.ToSingle(buf, 0x7C).ToString("F3");

            if (!I2CDriverLib.I2C_ReadReg(ref d, DEV_ADDR, 0xB0, 1, ref buf)) {
                DisconnectReset();
                return;
            }
            safetyStatusField.Value = buf[0];

            if (!I2CDriverLib.I2C_ReadReg(ref d, DEV_ADDR, 0xB1, 2, ref buf)) {
                DisconnectReset();
                return;
            }
            serrSourceLowField.Value = buf[0];
            serrSourceHighField.Value = buf[1];

            if (!I2CDriverLib.I2C_ReadReg(ref d, DEV_ADDR, 0xB2, 2, ref buf)) {
                DisconnectReset();
                return;
            }
            swarnSourceLowField.Value = buf[0];
            swarnSourceHighField.Value = buf[1];

            if (!pvddTargetBox.IsFocused) {
                if (!I2CDriverLib.I2C_ReadReg(ref d, DEV_ADDR, 0x20, 4, ref buf)) {
                    DisconnectReset();
                    return;
                }
                pvddTargetBox.Text = BitConverter.ToSingle(buf, 0).ToString("F2");
            }

            if (readAllCycles++ < READ_ALL_PERIOD) return;

            readAllCycles = 0;

            if (!I2CDriverLib.I2C_ReadReg(ref d, DEV_ADDR, 0x08, 1, ref buf)) {
                DisconnectReset();
                return;
            }
            controlField.Value = buf[0];

            if (!I2CDriverLib.I2C_ReadReg(ref d, DEV_ADDR, 0x10, 1, ref buf)) {
                DisconnectReset();
                return;
            }
            intMaskField.Value = buf[0];
        }

        private void DisconnectReset() {
            I2CDriverLib.i2c_disconnect(ref d);

            timer.Change(Timeout.Infinite, Timeout.Infinite);
            readAllCycles = READ_ALL_PERIOD;

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

                byte[] id = [0];
                if (I2CDriverLib.I2C_ReadReg(ref d, DEV_ADDR, 0xFF, 1, ref id)) {
                    idLabel.Content = $"0x{id[0]:X2}";

                    if (id[0] == DEVICE_ID_CORRECT) {
                        timer.Change(0, 1000);
                        readAllCycles = READ_ALL_PERIOD;
                    }
                } else {
                    idLabel.Content = "ERR";
                }
            } else {
                I2CDriverLib.i2c_disconnect(ref d);

                timer.Change(Timeout.Infinite, Timeout.Infinite);
                readAllCycles = READ_ALL_PERIOD;

                connLabel.Content = "Disconnected";
                idLabel.Content = "0x00";
                connPortBox.IsEnabled = true;
                connButton.Content = "Connect";
            }
        }

        private void OnWindowClosing(object sender, System.ComponentModel.CancelEventArgs e) {
            DisconnectReset();
        }

        private void DoControlApply(object sender, RoutedEventArgs e) {
            if (d.connected == 0) return;

            var buf = new byte[1] { controlField.SwitchedValue };

            if (!I2CDriverLib.I2C_WriteReg(ref d, DEV_ADDR, 0x08, buf)) {
                DisconnectReset();
                return;
            }

            controlField.ResetSwitches();

            if (I2CDriverLib.I2C_ReadReg(ref d, DEV_ADDR, 0x08, 1, ref buf)) {
                controlField.Value = buf[0];
            }
        }

        private void DoIntMaskApply(object sender, RoutedEventArgs e) {
            if (d.connected == 0) return;

            var buf = new byte[1] { intMaskField.SwitchedValue };

            if (!I2CDriverLib.I2C_WriteReg(ref d, DEV_ADDR, 0x10, buf)) {
                DisconnectReset();
                return;
            }

            intMaskField.ResetSwitches();

            if (I2CDriverLib.I2C_ReadReg(ref d, DEV_ADDR, 0x10, 1, ref buf)) {
                intMaskField.Value = buf[0];
            }
        }

        private void DoIntFlagsClear(object sender, RoutedEventArgs e) {
            if (d.connected == 0) return;

            var buf = new byte[1] { 0 };

            if (!I2CDriverLib.I2C_WriteReg(ref d, DEV_ADDR, 0x11, buf)) {
                DisconnectReset();
                return;
            }

            if (I2CDriverLib.I2C_ReadReg(ref d, DEV_ADDR, 0x11, 1, ref buf)) {
                intFlagsField.Value = buf[0];
            }
        }

        private void DoPVDDApply(object sender, RoutedEventArgs e) {
            if (d.connected == 0) return;

            if (!float.TryParse(pvddTargetBox.Text, out float value) || value < 18.7f || value > 53.5f) {
                MessageBox.Show("Invalid input, must be a number between 18.7 and 53.5");
                return;
            }

            var buf = BitConverter.GetBytes(value);

            if (!I2CDriverLib.I2C_WriteReg(ref d, DEV_ADDR, 0x20, buf)) {
                DisconnectReset();
                return;
            }

            if (I2CDriverLib.I2C_ReadReg(ref d, DEV_ADDR, 0x20, 4, ref buf)) {
                pvddTargetBox.Text = BitConverter.ToSingle(buf, 0).ToString("F2");
            }
        }
    }
}