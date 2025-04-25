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
    /// Interaction logic for DACWindow.xaml
    /// </summary>
    public partial class DACWindow : Window {

        private const byte DEV_ADDR = 0x1D;
        private const int READ_ALL_PERIOD = 5;
        private const byte DEVICE_ID_CORRECT = 0x8D;

        private bool i2c_connected = false;
        private readonly Timer timer;
        private int readAllCycles;
        private int errorCount;

        public DACWindow() {
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

            byte[] buf = new byte[256];

            if (!App.I2CCrcRead(DEV_ADDR, 0xFF, 1, ref buf)) {
                connLabel.Background = Brushes.Yellow;
                errorCount++;
                return;
            }
            if (buf[0] != DEVICE_ID_CORRECT) {
                DisconnectReset();
                return;
            }

            if (!App.I2CCrcRead(DEV_ADDR, 0x01, 1, ref buf)) {
                connLabel.Background = Brushes.Yellow;
                errorCount++;
                return;
            }
            statusField.Value = buf[0];

            if (!App.I2CCrcRead(DEV_ADDR, 0x11, 1, ref buf)) {
                connLabel.Background = Brushes.Yellow;
                errorCount++;
                return;
            }
            intFlagsField.Value = buf[0];

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

            int[] read_reg_sizes = [2, 1, 1, 1, 1, 1, 2];
            if (!App.I2CCrcMultiRead(DEV_ADDR, 0x20, read_reg_sizes, ref buf)) {
                connLabel.Background = Brushes.Yellow;
                errorCount++;
                return;
            }
            if (!volCh1Box.IsFocused && !volCh2Box.IsFocused) {
                volCh1Box.Text = (buf[0] * -.5f).ToString("F1");
                volCh2Box.Text = (buf[1] * -.5f).ToString("F1");
            }
            muteField.Value = buf[2];
            pathField.Value = buf[3];
            clockCfgField.Value = buf[4];
            if (!masterDivBox.IsFocused) {
                masterDivBox.Text = (buf[5] + 1).ToString();
            }
            if (!tdmSlotsBox.IsFocused) {
                tdmSlotsBox.Text = (buf[6] + 1).ToString();
            }
            if (!tdmSlotCh1Box.IsFocused && !tdmSlotCh2Box.IsFocused) {
                tdmSlotCh1Box.Text = (buf[7] + 1).ToString();
                tdmSlotCh2Box.Text = (buf[8] + 1).ToString();
            }

            read_reg_sizes = [1, 4, 4];
            if (!App.I2CCrcMultiRead(DEV_ADDR, 0x30, read_reg_sizes, ref buf)) {
                connLabel.Background = Brushes.Yellow;
                errorCount++;
                return;
            }
            if (buf[0] <= 7) {
                filterBox.SelectedIndex = buf[0];
            } else {
                filterBox.SelectedIndex = -1;
            }
            if (!thdC2Ch1Box.IsFocused && !thdC2Ch2Box.IsFocused) {
                thdC2Ch1Box.Text = BitConverter.ToInt16(buf, 1).ToString();
                thdC2Ch2Box.Text = BitConverter.ToInt16(buf, 3).ToString();
            }
            if (!thdC3Ch1Box.IsFocused && !thdC3Ch2Box.IsFocused) {
                thdC3Ch1Box.Text = BitConverter.ToInt16(buf, 5).ToString();
                thdC3Ch2Box.Text = BitConverter.ToInt16(buf, 7).ToString();
            }

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

        private void DoFilterApply(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            if (filterBox.SelectedIndex < 0 || filterBox.SelectedIndex > 7) {
                MessageBox.Show("Invalid filter shape selected");
                return;
            }

            var buf = new byte[1] { (byte)filterBox.SelectedIndex };

            if (!App.I2CCrcWrite(DEV_ADDR, 0x30, buf)) {
                connLabel.Background = Brushes.Yellow;
                errorCount++;
                return;
            }

            if (App.I2CCrcRead(DEV_ADDR, 0x30, 1, ref buf)) {
                if (buf[0] <= 7) {
                    filterBox.SelectedIndex = buf[0];
                } else {
                    filterBox.SelectedIndex = -1;
                }
            }
        }

        private void DoTHDC2Apply(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            if (!short.TryParse(thdC2Ch1Box.Text, out var c2ch1) || !short.TryParse(thdC2Ch2Box.Text, out var c2ch2)) {
                MessageBox.Show("Invalid coefficient values - must be valid signed 16-bit numbers");
                return;
            }

            byte[] buf = [.. BitConverter.GetBytes(c2ch1), .. BitConverter.GetBytes(c2ch2)];

            if (!App.I2CCrcWrite(DEV_ADDR, 0x31, buf)) {
                connLabel.Background = Brushes.Yellow;
                errorCount++;
                return;
            }

            if (App.I2CCrcRead(DEV_ADDR, 0x31, 4, ref buf)) {
                thdC2Ch1Box.Text = BitConverter.ToInt16(buf, 0).ToString();
                thdC2Ch2Box.Text = BitConverter.ToInt16(buf, 2).ToString();
            }
        }

        private void DoTHDC3Apply(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            if (!short.TryParse(thdC3Ch1Box.Text, out var c3ch1) || !short.TryParse(thdC3Ch2Box.Text, out var c3ch2)) {
                MessageBox.Show("Invalid coefficient values - must be valid signed 16-bit numbers");
                return;
            }

            byte[] buf = [.. BitConverter.GetBytes(c3ch1), .. BitConverter.GetBytes(c3ch2)];

            if (!App.I2CCrcWrite(DEV_ADDR, 0x32, buf)) {
                connLabel.Background = Brushes.Yellow;
                errorCount++;
                return;
            }

            if (App.I2CCrcRead(DEV_ADDR, 0x32, 4, ref buf)) {
                thdC3Ch1Box.Text = BitConverter.ToInt16(buf, 0).ToString();
                thdC3Ch2Box.Text = BitConverter.ToInt16(buf, 2).ToString();
            }
        }

        private void DoVolumeApply(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            if (!float.TryParse(volCh1Box.Text, out var vol1dB) || !float.TryParse(volCh2Box.Text, out var vol2dB)) {
                MessageBox.Show("Invalid volume: Must be a valid decimal number between -127.5 and 0 (inclusive)");
                return;
            }

            var vol1Int = Math.Floor(vol1dB / -.5f);
            var vol2Int = Math.Floor(vol2dB / -.5f);

            if (vol1Int < 0f || vol1Int > 255f || vol2Int < 0f || vol2Int > 255f) {
                MessageBox.Show("Invalid volume: Must be a valid decimal number between -127.5 and 0 (inclusive)");
                return;
            }

            byte[] buf = [(byte)vol1Int, (byte)vol2Int];

            if (!App.I2CCrcWrite(DEV_ADDR, 0x20, buf)) {
                connLabel.Background = Brushes.Yellow;
                errorCount++;
                return;
            }

            if (App.I2CCrcRead(DEV_ADDR, 0x20, 2, ref buf)) {
                volCh1Box.Text = (buf[0] * -.5f).ToString("F1");
                volCh2Box.Text = (buf[1] * -.5f).ToString("F1");
            }
        }

        private void DoMuteApply(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            var buf = new byte[1] { muteField.SwitchedValue };

            if (!App.I2CCrcWrite(DEV_ADDR, 0x21, buf)) {
                connLabel.Background = Brushes.Yellow;
                errorCount++;
                return;
            }

            muteField.ResetSwitches();

            if (App.I2CCrcRead(DEV_ADDR, 0x21, 1, ref buf)) {
                muteField.Value = buf[0];
            }
        }

        private void DoPathApply(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            var buf = new byte[1] { pathField.SwitchedValue };

            if (!App.I2CCrcWrite(DEV_ADDR, 0x22, buf)) {
                connLabel.Background = Brushes.Yellow;
                errorCount++;
                return;
            }

            pathField.ResetSwitches();

            if (App.I2CCrcRead(DEV_ADDR, 0x22, 1, ref buf)) {
                pathField.Value = buf[0];
            }
        }

        private void DoClockConfigApply(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            var buf = new byte[1] { clockCfgField.SwitchedValue };

            if (!App.I2CCrcWrite(DEV_ADDR, 0x23, buf)) {
                connLabel.Background = Brushes.Yellow;
                errorCount++;
                return;
            }

            clockCfgField.ResetSwitches();

            if (App.I2CCrcRead(DEV_ADDR, 0x23, 1, ref buf)) {
                clockCfgField.Value = buf[0];
            }
        }

        private void DoMasterDivApply(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            if (!ushort.TryParse(masterDivBox.Text, out var mdiv16) || mdiv16 < 1 || mdiv16 > 256) {
                MessageBox.Show("Invalid master divider - must be an integer between 1 and 256");
                return;
            }

            var buf = new byte[1] { (byte)(mdiv16 - 1) };

            if (!App.I2CCrcWrite(DEV_ADDR, 0x24, buf)) {
                connLabel.Background = Brushes.Yellow;
                errorCount++;
                return;
            }

            if (App.I2CCrcRead(DEV_ADDR, 0x24, 1, ref buf)) {
                masterDivBox.Text = (buf[0] + 1).ToString();
            }
        }

        private void DoTDMSlotsApply(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            if (!byte.TryParse(tdmSlotsBox.Text, out var slots) || slots < 1 || slots > 32) {
                MessageBox.Show("Invalid slot count - must be an integer between 1 and 32");
                return;
            }

            var buf = new byte[1] { (byte)(slots - 1) };

            if (!App.I2CCrcWrite(DEV_ADDR, 0x25, buf)) {
                connLabel.Background = Brushes.Yellow;
                errorCount++;
                return;
            }

            if (App.I2CCrcRead(DEV_ADDR, 0x25, 1, ref buf)) {
                tdmSlotsBox.Text = (buf[0] + 1).ToString();
            }
        }

        private void DoChannelSlotsApply(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            if (!byte.TryParse(tdmSlotCh1Box.Text, out var slot1) || slot1 < 1 || slot1 > 32 || !byte.TryParse(tdmSlotCh2Box.Text, out var slot2) || slot2 < 1 || slot2 > 32) {
                MessageBox.Show("Invalid slot number - must be an integer between 1 and 32");
                return;
            }

            var buf = new byte[2] { (byte)(slot1 - 1), (byte)(slot2 - 1) };

            if (!App.I2CCrcWrite(DEV_ADDR, 0x26, buf)) {
                connLabel.Background = Brushes.Yellow;
                errorCount++;
                return;
            }

            if (App.I2CCrcRead(DEV_ADDR, 0x26, 2, ref buf)) {
                tdmSlotCh1Box.Text = (buf[0] + 1).ToString();
                tdmSlotCh2Box.Text = (buf[1] + 1).ToString();
            }
        }
    }
}
