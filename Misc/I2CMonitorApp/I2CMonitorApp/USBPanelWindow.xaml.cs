using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.IO;
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

namespace I2CMonitorApp
{
    /// <summary>
    /// Interaction logic for USBPanelWindow.xaml
    /// </summary>
    public partial class USBPanelWindow : Window
    {
        private const byte TPS25751_ADDR = 0x21;
        private const byte EEPROM_ADDR = 0x50;

        private static readonly Brush PROGRESS_NORMAL_BRUSH = new SolidColorBrush(Color.FromRgb(6, 176, 37));
        private static readonly Brush PROGRESS_ERROR_BRUSH = new SolidColorBrush(Color.FromRgb(176, 6, 6));

        private bool i2ct_connected = false;
        private bool i2cc_connected = false;
        private readonly Timer timer;
        private int errorCount;

        public USBPanelWindow()
        {
            InitializeComponent();
            timer = new(OnTimerExpired);
            connPortBoxT.ItemsSource = SerialPort.GetPortNames();
            connPortBoxC.ItemsSource = SerialPort.GetPortNames();
            connPortBoxT.Text = App.i2c_portname;
            connPortBoxC.Text = App.i2c_portname;
            connLabelT.Background = Brushes.Transparent;
            connLabelC.Background = Brushes.Transparent;
            errorCount = 0;
        }

        void OnTimerExpired(object? state) {
            Dispatcher.Invoke(TimerTick);
        }

        private void TimerTick() {
            if (!i2ct_connected || App.i2cd.connected == 0) {
                if (!i2cc_connected || App.i2cd.connected == 0) DisconnectReset();
                return;
            }

            if (errorCount > 10) {
                DisconnectReset();
                return;
            }

            byte[] buf = new byte[16];

            //read mode
            if (!I2CDriverLib.I2C_ReadReg(ref App.i2cd, TPS25751_ADDR, 0x03, 5, ref buf)) {
                connLabelT.Background = Brushes.Yellow;
                errorCount++;
                return;
            }
            var modeStr = Encoding.ASCII.GetString(buf, 1, 4);
            modeBox.Text = modeStr;

            //read boot flags
            if (!I2CDriverLib.I2C_ReadReg(ref App.i2cd, TPS25751_ADDR, 0x2D, 6, ref buf)) {
                connLabelT.Background = Brushes.Yellow;
                errorCount++;
                return;
            }
            buf[6] = buf[7] = buf[8] = 0;
            bootFlagsBox.Text = BitConverter.ToUInt64(buf, 1).ToString("X10");

            //other registers only if we're in APP mode
            if (modeBox.Text == "APP ") {
                //read status
                if (!I2CDriverLib.I2C_ReadReg(ref App.i2cd, TPS25751_ADDR, 0x1A, 6, ref buf)) {
                    connLabelT.Background = Brushes.Yellow;
                    errorCount++;
                    return;
                }
                statusBox.Text = BitConverter.ToUInt32(buf, 1).ToString("X8");

                //read power path status
                if (!I2CDriverLib.I2C_ReadReg(ref App.i2cd, TPS25751_ADDR, 0x26, 6, ref buf)) {
                    connLabelT.Background = Brushes.Yellow;
                    errorCount++;
                    return;
                }
                buf[6] = buf[7] = buf[8] = 0;
                ppStatusBox.Text = BitConverter.ToUInt64(buf, 1).ToString("X10");

                //read active PDO
                if (!I2CDriverLib.I2C_ReadReg(ref App.i2cd, TPS25751_ADDR, 0x34, 7, ref buf)) {
                    connLabelT.Background = Brushes.Yellow;
                    errorCount++;
                    return;
                }
                buf[7] = buf[8] = 0;
                activePDOBox.Text = BitConverter.ToUInt64(buf, 1).ToString("X12");

                //read active RDO
                if (!I2CDriverLib.I2C_ReadReg(ref App.i2cd, TPS25751_ADDR, 0x35, 13, ref buf)) {
                    connLabelT.Background = Brushes.Yellow;
                    errorCount++;
                    return;
                }
                activeRDOBox.Text = BitConverter.ToUInt32(buf, 1).ToString("X8");

                //read power status
                if (!I2CDriverLib.I2C_ReadReg(ref App.i2cd, TPS25751_ADDR, 0x3F, 3, ref buf)) {
                    connLabelT.Background = Brushes.Yellow;
                    errorCount++;
                    return;
                }
                powerStatusBox.Text = BitConverter.ToUInt16(buf, 1).ToString("X4");

                //read PD status
                if (!I2CDriverLib.I2C_ReadReg(ref App.i2cd, TPS25751_ADDR, 0x40, 5, ref buf)) {
                    connLabelT.Background = Brushes.Yellow;
                    errorCount++;
                    return;
                }
                pdStatusBox.Text = BitConverter.ToUInt32(buf, 1).ToString("X8");
            } else {
                //non-APP status: clear unavailable boxes
                statusBox.Text = ppStatusBox.Text = activePDOBox.Text = activeRDOBox.Text = powerStatusBox.Text = pdStatusBox.Text = "---";
            }

            connLabelT.Background = Brushes.Transparent;
            errorCount = 0;
        }

        private void DisconnectReset() {
            i2ct_connected = false;
            i2cc_connected = false;
            App.I2CDisconnect(this);

            timer.Change(Timeout.Infinite, Timeout.Infinite);

            connLabelT.Content = "Disconnected";
            connLabelC.Content = "Disconnected";
            connPortBoxT.IsEnabled = true;
            connPortBoxC.IsEnabled = true;
            connButtonT.Content = "Connect";
            connButtonC.Content = "Connect";
            connLabelT.Background = Brushes.Transparent;
            connLabelC.Background = Brushes.Transparent;
            errorCount = 0;
        }

        private void OnConnectButtonTClick(object sender, RoutedEventArgs e) {
            if (!i2ct_connected || App.i2cd.connected == 0) {
                DisconnectReset();

                string portname = App.i2c_portname;
                if (App.i2cd.connected == 0) {
                    if (connPortBoxT.SelectedIndex < 0) {
                        MessageBox.Show("Please select a port!");
                        return;
                    }
                    portname = (string)connPortBoxT.SelectedValue;
                }

                if (!App.I2CConnect(this, portname)) {
                    MessageBox.Show("Connection failed...");
                    DisconnectReset();
                    return;
                }

                i2ct_connected = true;
                connLabelT.Content = "Connected";
                connPortBoxT.IsEnabled = false;
                connButtonT.Content = "Disconnect";

                timer.Change(0, 1000);
            } else {
                DisconnectReset();
            }
        }

        private void OnConnectButtonCClick(object sender, RoutedEventArgs e) {
            if (!i2cc_connected || App.i2cd.connected == 0) {
                DisconnectReset();

                string portname = App.i2c_portname;
                if (App.i2cd.connected == 0) {
                    if (connPortBoxC.SelectedIndex < 0) {
                        MessageBox.Show("Please select a port!");
                        return;
                    }
                    portname = (string)connPortBoxC.SelectedValue;
                }

                if (!App.I2CConnect(this, portname)) {
                    MessageBox.Show("Connection failed...");
                    DisconnectReset();
                    return;
                }

                i2cc_connected = true;
                connLabelC.Content = "Connected";
                connPortBoxC.IsEnabled = false;
                connButtonC.Content = "Disconnect";

                //timer.Change(0, 1000);
            } else {
                DisconnectReset();
            }
        }

        private void DoManualRead(object sender, RoutedEventArgs e) {
            if (!i2ct_connected || App.i2cd.connected == 0) return;

            if (!byte.TryParse(manAddrBox.Text, System.Globalization.NumberStyles.HexNumber, null, out var addr)) {
                MessageBox.Show("Address must be in [0x00, 0xFF]!");
                return;
            }

            if (!byte.TryParse(manSizeBox.Text, System.Globalization.NumberStyles.HexNumber, null, out var size) || size < 1 || size > 254) {
                MessageBox.Show("Size must be in [1, 254]!");
                return;
            }

            byte[] buf = new byte[size + 1];

            if (!I2CDriverLib.I2C_ReadReg(ref App.i2cd, TPS25751_ADDR, addr, size + 1, ref buf)) {
                MessageBox.Show("Read failed!");
                return;
            }

            var readSize = buf[0];

            StringBuilder sb = new();
            foreach (var b in buf.Skip(1).Take(readSize).Reverse()) {
                sb.Append(b.ToString("X2"));
            }
            manDataBox.Text = sb.ToString();
        }

        private void DoManualWrite(object sender, RoutedEventArgs e) {
            if (!i2ct_connected || App.i2cd.connected == 0) return;

            if (!byte.TryParse(manAddrBox.Text, System.Globalization.NumberStyles.HexNumber, null, out var addr)) {
                MessageBox.Show("Address must be in [0x00, 0xFF]!");
                return;
            }

            if (!byte.TryParse(manSizeBox.Text, System.Globalization.NumberStyles.HexNumber, null, out var size) || size < 1 || size > 254) {
                MessageBox.Show("Size must be in [1, 254]!");
                return;
            }

            byte[] buf = new byte[size + 1];
            Array.Fill<byte>(buf, 0);
            buf[0] = size;

            var dataStr = new string([.. manDataBox.Text.Where(c => !char.IsWhiteSpace(c))]);
            int dataPtr = 1;
            for (int i = dataStr.Length - 2; i >= 0; i -= 2) {
                if (!byte.TryParse(dataStr.AsSpan(i, 2), System.Globalization.NumberStyles.HexNumber, null, out var b)) {
                    MessageBox.Show("Data must be a valid hexadecimal byte string!");
                    return;
                }
                buf[dataPtr++] = b;
                if (dataPtr > size) break;
            }

            if (!I2CDriverLib.I2C_WriteReg(ref App.i2cd, TPS25751_ADDR, addr, buf)) {
                MessageBox.Show("Write failed!");
                return;
            }
        }

        private async Task<bool> WriteROMPage(ushort addr, byte[] data) {
            if (addr % 128 != 0 || data.Length > 128) {
                return false;
            }

            var writeBuf = new byte[130];
            writeBuf[0] = (byte)(addr >> 8);
            writeBuf[1] = (byte)(addr & 0xFF);
            data.CopyTo(writeBuf.AsSpan(2));

            int r = I2CDriverLib.i2c_start(ref App.i2cd, EEPROM_ADDR, 0);
            if (r == 0) {
                I2CDriverLib.i2c_stop(ref App.i2cd);
                I2CDriverLib.i2c_getstatus(ref App.i2cd);
                return false;
            }

            r = I2CDriverLib.i2c_write(ref App.i2cd, writeBuf, 130);
            I2CDriverLib.i2c_stop(ref App.i2cd);
            I2CDriverLib.i2c_getstatus(ref App.i2cd);
            if (r == 0) {
                return false;
            }

            //ACK polling to check for write finish
            for (int i = 0; i < 10; i++) {
                r = I2CDriverLib.i2c_start(ref App.i2cd, EEPROM_ADDR, 0);
                I2CDriverLib.i2c_stop(ref App.i2cd);
                if (r > 0) {
                    return true;
                }

                await Task.Delay(1);
            }

            return false;
        }

        private async Task<byte[]?> ReadNextROMHalfPage(int maxLength) {
            var length = Math.Min(maxLength, 64);
            return await Task.Run(() => {
                var readBuf = new byte[length];

                int r = I2CDriverLib.i2c_start(ref App.i2cd, EEPROM_ADDR, 1);
                if (r == 0) {
                    I2CDriverLib.i2c_stop(ref App.i2cd);
                    I2CDriverLib.i2c_getstatus(ref App.i2cd);
                    return null;
                }

                I2CDriverLib.i2c_read(ref App.i2cd, readBuf, (nuint)length);
                I2CDriverLib.i2c_stop(ref App.i2cd);
                I2CDriverLib.i2c_getstatus(ref App.i2cd);

                return readBuf;
            });
        }

        private async void DoROMWrite(object sender, RoutedEventArgs e) {
            if (!i2cc_connected || App.i2cd.connected == 0) return;

            var path = romFileBox.Text;
            if (!File.Exists(path)) {
                MessageBox.Show("Given file does not exist!");
                return;
            }

            if (new FileInfo(path).Length > 65536) {
                MessageBox.Show("Given file is too large (max 64KB)!");
                return;
            }

            byte[] writeData = File.ReadAllBytes(path);

            romStartButton.IsEnabled = false;
            romStatusLabel.Content = $"Writing (0 / {writeData.Length})";
            romWriteBar.Foreground = PROGRESS_NORMAL_BRUSH;
            romWriteBar.Value = 0;
            romWriteLabel.Content = "0%";
            romVerifyBar.Foreground = PROGRESS_NORMAL_BRUSH;
            romVerifyBar.Value = 0;
            romVerifyLabel.Content = "0%";

            //write
            int pages = (writeData.Length + 127) / 128;
            for (int page = 0; page < pages; page++) {
                if (await WriteROMPage((ushort)(128 * page), [.. writeData.Skip(128 * page).Take(128)])) {
                    int newProgress = (page + 1) * 100 / pages;
                    romStatusLabel.Content = $"Writing ({128 * (page + 1)} / {writeData.Length})";
                    romWriteBar.Value = newProgress;
                    romWriteLabel.Content = $"{newProgress}%";
                } else {
                    romStatusLabel.Content = $"WRITE FAILED (completed {128 * page} / {writeData.Length})";
                    romWriteBar.Foreground = PROGRESS_ERROR_BRUSH;
                    romStartButton.IsEnabled = true;
                    return;
                }
            }

            //verify - begin by going back to address 0
            int r = I2CDriverLib.i2c_start(ref App.i2cd, EEPROM_ADDR, 0);
            if (r == 0) {
                I2CDriverLib.i2c_stop(ref App.i2cd);
                I2CDriverLib.i2c_getstatus(ref App.i2cd);
                romStatusLabel.Content = "FAILED TO START VERIFICATION";
                romStartButton.IsEnabled = true;
                return;
            }

            r = I2CDriverLib.i2c_write(ref App.i2cd, [0, 0], 2);
            I2CDriverLib.i2c_stop(ref App.i2cd);
            I2CDriverLib.i2c_getstatus(ref App.i2cd);
            if (r == 0) {
                romStatusLabel.Content = "FAILED TO START VERIFICATION";
                romStartButton.IsEnabled = true;
                return;
            }

            romStatusLabel.Content = $"Verifying (0 / {writeData.Length})";

            //do actual verification
            int halfpages = (writeData.Length + 63) / 64;
            for (int halfpage = 0; halfpage < halfpages; halfpage++) {
                var hp_data = await ReadNextROMHalfPage(writeData.Length - 64 * halfpage);
                if (hp_data != null) {
                    //check half-page data
                    if (hp_data.SequenceEqual(writeData.Skip(64 * halfpage).Take(64))) {
                        int newProgress = (halfpage + 1) * 100 / halfpages;
                        romStatusLabel.Content = $"Verifying ({64 * (halfpage + 1)} / {writeData.Length})";
                        romVerifyBar.Value = newProgress;
                        romVerifyLabel.Content = $"{newProgress}%";
                    } else {
                        romStatusLabel.Content = $"VERIFY DATA MISMATCH (completed {64 * halfpage} / {writeData.Length})";
                        romVerifyBar.Foreground = PROGRESS_ERROR_BRUSH;
                        romStartButton.IsEnabled = true;
                        return;
                    }
                } else {
                    romStatusLabel.Content = $"VERIFY READ FAILED (completed {64 * halfpage} / {writeData.Length})";
                    romVerifyBar.Foreground = PROGRESS_ERROR_BRUSH;
                    romStartButton.IsEnabled = true;
                    return;
                }
            }

            //success
            romStatusLabel.Content = $"Write successful";
            romStartButton.IsEnabled = true;
        }

        private void DoROMManualRead(object sender, RoutedEventArgs e) {
            if (!i2cc_connected || App.i2cd.connected == 0) return;

            if (!ushort.TryParse(romManAddrBox.Text, System.Globalization.NumberStyles.HexNumber, null, out var addr)) {
                MessageBox.Show("Address must be in [0x0000, 0xFFFF]!");
                return;
            }

            var addrBuf = new byte[2] { (byte)(addr >> 8), (byte)(addr & 0xFF) };
            var readBuf = new byte[4];

            int r = I2CDriverLib.i2c_start(ref App.i2cd, EEPROM_ADDR, 0);
            if (r == 0) {
                I2CDriverLib.i2c_stop(ref App.i2cd);
                I2CDriverLib.i2c_getstatus(ref App.i2cd);
                romManDataBox.Text = "START ERR";
                return;
            }

            r = I2CDriverLib.i2c_write(ref App.i2cd, addrBuf, 2);
            if (r == 0) {
                I2CDriverLib.i2c_stop(ref App.i2cd);
                I2CDriverLib.i2c_getstatus(ref App.i2cd);
                romManDataBox.Text = "ADDR WRITE ERR";
                return;
            }

            r = I2CDriverLib.i2c_start(ref App.i2cd, EEPROM_ADDR, 1);
            if (r == 0) {
                I2CDriverLib.i2c_stop(ref App.i2cd);
                I2CDriverLib.i2c_getstatus(ref App.i2cd);
                romManDataBox.Text = "RESTART ERR";
                return;
            }

            I2CDriverLib.i2c_read(ref App.i2cd, readBuf, 4);
            I2CDriverLib.i2c_stop(ref App.i2cd);
            I2CDriverLib.i2c_getstatus(ref App.i2cd);

            string res = readBuf[0].ToString("X2");
            for (int i = 1; i < 4; i++) {
                res += " " + readBuf[i].ToString("X2");
            }
            romManDataBox.Text = res;
        }

        private void SelectROMFile(object sender, RoutedEventArgs e) {
            OpenFileDialog dialog = new();
            if (dialog.ShowDialog() == true) {
                romFileBox.Text = dialog.FileName;
            }
        }
    }
}
