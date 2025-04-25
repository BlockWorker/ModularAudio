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
    public partial class AmpWindow : Window {

        private const byte DEV_ADDR = 0x11;
        private const int READ_ALL_PERIOD = 5;
        private const byte DEVICE_ID_CORRECT = 0xA1;

        private bool i2c_connected = false;
        private readonly Timer timer;
        private int readAllCycles;
        private int errorCount;

        public AmpWindow() {
            InitializeComponent();
            timer = new(OnTimerExpired);
            readAllCycles = READ_ALL_PERIOD;
            connPortBox.ItemsSource = SerialPort.GetPortNames();
            connPortBox.Text = App.i2c_portname;
            connLabel.Background = Brushes.Transparent;
            errorCount = 0;
        }

        void OnTimerExpired(object? state) {
            Dispatcher.Invoke(TimerTick);
        }

        private void TimerTick() {
            if (!i2c_connected || App.i2cd.connected == 0) {
                DisconnectReset();
                return;
            }

            if (errorCount > 10) {
                DisconnectReset();
                return;
            }

            var buf = new byte[256];

            if (!App.I2CCrcRead(DEV_ADDR, 0xFF, 1, ref buf)) {
                connLabel.Background = Brushes.Yellow;
                errorCount++;
                return;
            }
            if (buf[0] != DEVICE_ID_CORRECT) {
                DisconnectReset();
                return;
            }

            if (!App.I2CCrcRead(DEV_ADDR, 0x01, 2, ref buf)) {
                connLabel.Background = Brushes.Yellow;
                errorCount++;
                return;
            }
            statusLowField.Value = buf[0];
            statusHighField.Value = buf[1];

            if (!App.I2CCrcRead(DEV_ADDR, 0x11, 1, ref buf)) {
                connLabel.Background = Brushes.Yellow;
                errorCount++;
                return;
            }
            intFlagsField.Value = buf[0];

            int[] read_reg_sizes = [4, 4];
            if (!App.I2CCrcMultiRead(DEV_ADDR, 0x21, read_reg_sizes, ref buf)) {
                connLabel.Background = Brushes.Yellow;
                errorCount++;
                return;
            }
            pvddReqBox.Text = BitConverter.ToSingle(buf, 0).ToString("F2");
            pvddMeasBox.Text = BitConverter.ToSingle(buf, 4).ToString("F2");

            read_reg_sizes = Enumerable.Repeat(4, 32).ToArray();
            if (!App.I2CCrcMultiRead(DEV_ADDR, 0x30, read_reg_sizes, ref buf)) {
                connLabel.Background = Brushes.Yellow;
                errorCount++;
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

            read_reg_sizes = [1, 2, 2];
            if (!App.I2CCrcMultiRead(DEV_ADDR, 0xB0, read_reg_sizes, ref buf)) {
                connLabel.Background = Brushes.Yellow;
                errorCount++;
                return;
            }
            safetyStatusField.Value = buf[0];
            serrSourceLowField.Value = buf[1];
            serrSourceHighField.Value = buf[2];
            swarnSourceLowField.Value = buf[3];
            swarnSourceHighField.Value = buf[4];

            if (!pvddTargetBox.IsFocused) {
                if (!App.I2CCrcRead(DEV_ADDR, 0x20, 4, ref buf)) {
                    connLabel.Background = Brushes.Yellow;
                    errorCount++;
                    return;
                }
                pvddTargetBox.Text = BitConverter.ToSingle(buf, 0).ToString("F2");
            }

            if (readAllCycles++ < READ_ALL_PERIOD) {
                connLabel.Background = Brushes.Transparent;
                errorCount = 0;
                return;
            }

            readAllCycles = 0;

            if (!App.I2CCrcRead(DEV_ADDR, 0x08, 1, ref buf)) {
                connLabel.Background = Brushes.Yellow;
                errorCount++;
                return;
            }
            controlField.Value = buf[0];

            if (!App.I2CCrcRead(DEV_ADDR, 0x10, 1, ref buf)) {
                connLabel.Background = Brushes.Yellow;
                errorCount++;
                return;
            }
            intMaskField.Value = buf[0];

            connLabel.Background = Brushes.Transparent;
            errorCount = 0;
        }

        private void DisconnectReset() {
            i2c_connected = false;
            App.I2CDisconnect(this);

            timer.Change(Timeout.Infinite, Timeout.Infinite);
            readAllCycles = READ_ALL_PERIOD;

            connLabel.Content = "Disconnected";
            idLabel.Content = "0x00";
            connPortBox.IsEnabled = true;
            connButton.Content = "Connect";
            connLabel.Background = Brushes.Transparent;
            errorCount = 0;
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

                byte[] id = [0];
                if (App.I2CCrcRead(DEV_ADDR, 0xFF, 1, ref id)) {
                    idLabel.Content = $"0x{id[0]:X2}";

                    if (id[0] == DEVICE_ID_CORRECT) {
                        timer.Change(0, 1000);
                        readAllCycles = READ_ALL_PERIOD;
                    }
                } else {
                    idLabel.Content = "ERR";
                }
            } else {
                DisconnectReset();
            }
        }

        private void OnWindowClosing(object sender, System.ComponentModel.CancelEventArgs e) {
            DisconnectReset();
        }

        private void DoControlApply(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            var buf = new byte[1] { controlField.SwitchedValue };

            if (!App.I2CCrcWrite(DEV_ADDR, 0x08, buf)) {
                connLabel.Background = Brushes.Yellow;
                errorCount++;
                return;
            }

            controlField.ResetSwitches();

            if (App.I2CCrcRead(DEV_ADDR, 0x08, 1, ref buf)) {
                controlField.Value = buf[0];
            }
        }

        private void DoIntMaskApply(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            var buf = new byte[1] { intMaskField.SwitchedValue };

            if (!App.I2CCrcWrite(DEV_ADDR, 0x10, buf)) {
                connLabel.Background = Brushes.Yellow;
                errorCount++;
                return;
            }

            intMaskField.ResetSwitches();

            if (App.I2CCrcRead(DEV_ADDR, 0x10, 1, ref buf)) {
                intMaskField.Value = buf[0];
            }
        }

        private void DoIntFlagsClear(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            var buf = new byte[1] { 0 };

            if (!App.I2CCrcWrite(DEV_ADDR, 0x11, buf)) {
                connLabel.Background = Brushes.Yellow;
                errorCount++;
                return;
            }

            if (App.I2CCrcRead(DEV_ADDR, 0x11, 1, ref buf)) {
                intFlagsField.Value = buf[0];
            }
        }

        private void DoPVDDApply(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            if (!float.TryParse(pvddTargetBox.Text, out float value) || value < 18.7f || value > 53.5f) {
                MessageBox.Show("Invalid input, must be a number between 18.7 and 53.5");
                return;
            }

            var buf = BitConverter.GetBytes(value);

            if (!App.I2CCrcWrite(DEV_ADDR, 0x20, buf)) {
                connLabel.Background = Brushes.Yellow;
                errorCount++;
                return;
            }

            if (App.I2CCrcRead(DEV_ADDR, 0x20, 4, ref buf)) {
                pvddTargetBox.Text = BitConverter.ToSingle(buf, 0).ToString("F2");
            }
        }
    }
}