using System;
using System.Collections.Generic;
using System.IO.Ports;
using System.Linq;
using System.Printing;
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
    /// Interaction logic for RTCWindow.xaml
    /// </summary>
    public partial class RTCWindow : Window {
        private const byte RTC_ADDR = 0x68;
        private const byte EEPROM_ADDR = 0x57;

        private bool i2c_connected = false;
        private readonly Timer timer;

        public RTCWindow() {
            InitializeComponent();
            timer = new(OnTimerExpired);
            connPortBox.ItemsSource = SerialPort.GetPortNames();
            connPortBox.Text = App.i2c_portname;
        }

        void OnTimerExpired(object? state) {
            Dispatcher.Invoke(TimerTick);
        }

        private static byte ByteToBCD(byte b) {
            if (b > 99) throw new ArgumentException("Invalid byte for BCD representation!");
            return (byte)(b % 10 | (b / 10) << 4);
        }

        private static byte BCDToByte(byte bcd) {
            return (byte)((bcd & 0xF) + 10 * (bcd >> 4));
        }

        private void TimerTick() {
            if (!i2c_connected || App.i2cd.connected == 0) {
                DisconnectReset();
                return;
            }

            byte[] buf = new byte[0x13];

            if (!I2CDriverLib.I2C_ReadReg(ref App.i2cd, RTC_ADDR, 0x00, 0x13, ref buf)) {
                DisconnectReset();
                return;
            }

            controlField.Value = buf[0x0E];
            controlStatusField.Value = buf[0x0F];

            if (!agingOffsetBox.IsFocused) {
                agingOffsetBox.Text = ((sbyte)buf[0x10]).ToString();
            }

            tempBox.Text = ((sbyte)buf[0x11] + buf[0x12] / 256.0f).ToString("F2");

            if (!yearBox.IsFocused && !monthBox.IsFocused && !dateBox.IsFocused && !dayBox.IsFocused) {
                //century 0 = 2000, 1 = 2100
                yearBox.Text = (2000 + 100 * (buf[0x05] >> 7) + BCDToByte(buf[0x06])).ToString("D4");
                monthBox.Text = BCDToByte((byte)(buf[0x05] & 0x1F)).ToString("D2");
                dateBox.Text = BCDToByte((byte)(buf[0x04] & 0x3F)).ToString("D2");
                dayBox.SelectedIndex = (buf[0x03] & 0x7) - 1;
            }

            if (!hourBox.IsFocused && !minBox.IsFocused && !secBox.IsFocused && !ampmBox.IsFocused) {
                if ((buf[0x02] & 0x40) != 0) {
                    //12 hour format
                    ampmBox.SelectedIndex = ((buf[0x02] & 0x20) != 0) ? 2 : 1;
                    hourBox.Text = BCDToByte((byte)(buf[0x02] & 0x1F)).ToString("D2");
                } else {
                    //24 hour format
                    ampmBox.SelectedIndex = 0;
                    hourBox.Text = BCDToByte((byte)(buf[0x02] & 0x3F)).ToString("D2");
                }
                minBox.Text = BCDToByte((byte)(buf[0x01] & 0x7F)).ToString("D2");
                secBox.Text = BCDToByte((byte)(buf[0x00] & 0x7F)).ToString("D2");
            }

            if (!a1DayDateBox.IsFocused && !a1HourBox.IsFocused && !a1MinBox.IsFocused && !a1SecBox.IsFocused && !a1ModeBox.IsFocused) {
                bool dayMode = (buf[0x0A] & 0x40) != 0;
                if (dayMode) {
                    //day mode
                    a1DayDateBox.Text = (buf[0x0A] & 0x07).ToString();
                } else {
                    //date mode
                    a1DayDateBox.Text = BCDToByte((byte)(buf[0x0A] & 0x3F)).ToString("D2");
                }
                if ((buf[0x09] & 0x40) != 0) {
                    //12 hour format
                    a1AmpmBox.SelectedIndex = ((buf[0x09] & 0x20) != 0) ? 2 : 1;
                    a1HourBox.Text = BCDToByte((byte)(buf[0x09] & 0x1F)).ToString("D2");
                } else {
                    //24 hour format
                    a1AmpmBox.SelectedIndex = 0;
                    a1HourBox.Text = BCDToByte((byte)(buf[0x09] & 0x3F)).ToString("D2");
                }
                a1MinBox.Text = BCDToByte((byte)(buf[0x08] & 0x7F)).ToString("D2");
                a1SecBox.Text = BCDToByte((byte)(buf[0x07] & 0x7F)).ToString("D2");
                if ((buf[0x0A] & 0x80) == 0) {
                    //day/date match mode
                    a1ModeBox.SelectedIndex = (dayMode ? 0 : 1);
                } else if ((buf[0x09] & 0x80) == 0) {
                    //hour match mode
                    a1ModeBox.SelectedIndex = 2;
                } else if ((buf[0x08] & 0x80) == 0) {
                    //minute match mode
                    a1ModeBox.SelectedIndex = 3;
                } else if ((buf[0x07] & 0x80) == 0) {
                    //second match mode
                    a1ModeBox.SelectedIndex = 4;
                } else {
                    //every second mode
                    a1ModeBox.SelectedIndex = 5;
                }
            }

            if (!a2DayDateBox.IsFocused && !a2HourBox.IsFocused && !a2MinBox.IsFocused && !a2ModeBox.IsFocused) {
                bool dayMode = (buf[0x0D] & 0x40) != 0;
                if (dayMode) {
                    //day mode
                    a2DayDateBox.Text = (buf[0x0D] & 0x07).ToString();
                } else {
                    //date mode
                    a2DayDateBox.Text = BCDToByte((byte)(buf[0x0D] & 0x3F)).ToString("D2");
                }
                if ((buf[0x0C] & 0x40) != 0) {
                    //12 hour format
                    a2AmpmBox.SelectedIndex = ((buf[0x0C] & 0x20) != 0) ? 2 : 1;
                    a2HourBox.Text = BCDToByte((byte)(buf[0x0C] & 0x1F)).ToString("D2");
                } else {
                    //24 hour format
                    a2AmpmBox.SelectedIndex = 0;
                    a2HourBox.Text = BCDToByte((byte)(buf[0x0C] & 0x3F)).ToString("D2");
                }
                a2MinBox.Text = BCDToByte((byte)(buf[0x0B] & 0x7F)).ToString("D2");
                if ((buf[0x0D] & 0x80) == 0) {
                    //day/date match mode
                    a2ModeBox.SelectedIndex = (dayMode ? 0 : 1);
                } else if ((buf[0x0C] & 0x80) == 0) {
                    //hour match mode
                    a2ModeBox.SelectedIndex = 2;
                } else if ((buf[0x0B] & 0x80) == 0) {
                    //minute match mode
                    a2ModeBox.SelectedIndex = 3;
                } else {
                    //every minute mode
                    a2ModeBox.SelectedIndex = 4;
                }
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

        private void OnWindowClosing(object sender, System.ComponentModel.CancelEventArgs e) {
            DisconnectReset();
        }

        private void DoControlApply(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            var buf = new byte[1] { controlField.SwitchedValue };

            if (!I2CDriverLib.I2C_WriteReg(ref App.i2cd, RTC_ADDR, 0x0E, buf)) {
                DisconnectReset();
                return;
            }

            controlField.ResetSwitches();

            TimerTick();
        }

        private void DoControlStatusApply(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            var buf = new byte[1] { controlStatusField.SwitchedValue };

            if (!I2CDriverLib.I2C_WriteReg(ref App.i2cd, RTC_ADDR, 0x0F, buf)) {
                DisconnectReset();
                return;
            }

            controlStatusField.ResetSwitches();

            TimerTick();
        }

        private void DoAgingOffsetApply(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            if (!sbyte.TryParse(agingOffsetBox.Text, out var offset)) {
                MessageBox.Show("Offset must be an integer in [-128, 127]!");
                return;
            }

            var buf = new byte[1] { (byte)offset };

            if (!I2CDriverLib.I2C_WriteReg(ref App.i2cd, RTC_ADDR, 0x10, buf)) {
                DisconnectReset();
                return;
            }

            TimerTick();
        }

        private void DoDateApply(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            if (!ushort.TryParse(yearBox.Text, out var year) || year < 2000 || year > 2199) {
                MessageBox.Show("Year must be in [2000, 2199]!");
                return;
            }
            if (!byte.TryParse(monthBox.Text, out var month) || month < 1 || month > 12) {
                MessageBox.Show("Month must be in [1, 12]!");
                return;
            }
            if (!byte.TryParse(dateBox.Text, out var date) || date < 1 || date > 31) {
                MessageBox.Show("Date must be in [1, 31]!");
                return;
            }
            try {
                DateOnly d = new(year, month, date);
                dayBox.SelectedIndex = (d.DayOfWeek == DayOfWeek.Sunday) ? 6 : (int)d.DayOfWeek - 1;
            } catch {
                MessageBox.Show("The given date does not exist on the calendar!");
                return;
            }

            var buf = new byte[4] {
                (byte)((dayBox.SelectedIndex + 1) & 0x7),
                (byte)(ByteToBCD(date) & 0x3F),
                (byte)(((year > 2099) ? 0x80 : 0x00) | (ByteToBCD(month) & 0x1F)),
                ByteToBCD((byte)(year % 100))
            };

            if (!I2CDriverLib.I2C_WriteReg(ref App.i2cd, RTC_ADDR, 0x03, buf)) {
                DisconnectReset();
                return;
            }

            TimerTick();
        }

        private void DoTimeApply(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            if (ampmBox.SelectedIndex < 0 || ampmBox.SelectedIndex > 2) {
                MessageBox.Show("Invalid AM/PM/24h selection!");
                return;
            }
            bool is24h = (ampmBox.SelectedIndex == 0);
            if (!byte.TryParse(hourBox.Text, out var hour) || (is24h && hour > 23) || (!is24h && (hour < 1 || hour > 12))) {
                MessageBox.Show("Hour is not valid for the given format (12h or 24h)!");
                return;
            }
            if (!byte.TryParse(minBox.Text, out var min) || min > 59) {
                MessageBox.Show("Minute must be in [0, 59]!");
                return;
            }
            if (!byte.TryParse(secBox.Text, out var sec) || sec > 59) {
                MessageBox.Show("Second must be in [0, 59]!");
                return;
            }

            var buf = new byte[3] {
                (byte)(ByteToBCD(sec) & 0x7F),
                (byte)(ByteToBCD(min) & 0x7F),
                (byte)((is24h ? 0x00 : 0x40) | ((ampmBox.SelectedIndex == 2) ? 0x20 : 0x00) | (ByteToBCD(hour) & (is24h ? 0x3F : 0x1F)))
            };

            if (!I2CDriverLib.I2C_WriteReg(ref App.i2cd, RTC_ADDR, 0x00, buf)) {
                DisconnectReset();
                return;
            }

            TimerTick();
        }

        private void DoAlarmApply(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            int index;
            switch (((Button)sender).Tag) {
                case "1":
                    index = 1;
                    break;
                case "2":
                    index = 2;
                    break;
                default:
                    return;
            }

            var aDayDateBox = (index == 1) ? a1DayDateBox : a2DayDateBox;
            var aAmpmBox = (index == 1) ? a1AmpmBox : a2AmpmBox;
            var aHourBox = (index == 1) ? a1HourBox : a2HourBox;
            var aMinBox = (index == 1) ? a1MinBox : a2MinBox;
            var aSecBox = (index == 1) ? a1SecBox : null;
            var aModeBox = (index == 1) ? a1ModeBox : a2ModeBox;

            if (aModeBox.SelectedIndex < 0 || aModeBox.SelectedIndex > ((index == 1) ? 5 : 4)) {
                MessageBox.Show("Invalid alarm mode selection!");
                return;
            }
            var dayMode = aModeBox.SelectedIndex == 0;
            if (!byte.TryParse(aDayDateBox.Text, out var dayDate) || (dayMode && dayDate > 7) || (!dayMode && dayDate > 31)) {
                MessageBox.Show("Alarm day/date field is not valid for the given mode!");
                return;
            }
            bool is24h = (aAmpmBox.SelectedIndex == 0);
            if (!byte.TryParse(aHourBox.Text, out var hour) || (is24h && hour > 23) || (!is24h && (hour < 1 || hour > 12))) {
                MessageBox.Show("Alarm hour is not valid for the given format (12h or 24h)!");
                return;
            }
            if (!byte.TryParse(aMinBox.Text, out var min) || min > 59) {
                MessageBox.Show("Alarm minute must be in [0, 59]!");
                return;
            }
            byte sec = 0;
            if (aSecBox != null) {
                if (!byte.TryParse(aSecBox.Text, out sec) || sec > 59) {
                    MessageBox.Show("Alarm second must be in [0, 59]!");
                    return;
                }
            }

            var buf = new byte[3] {
                (byte)(((aModeBox.SelectedIndex >= 4) ? 0x80 : 0x00) | ByteToBCD(min) & 0x7F),
                (byte)(((aModeBox.SelectedIndex >= 3) ? 0x80 : 0x00) | (is24h ? 0x00 : 0x40) | ((aAmpmBox.SelectedIndex == 2) ? 0x20 : 0x00) | (ByteToBCD(hour) & (is24h ? 0x3F : 0x1F))),
                (byte)(((aModeBox.SelectedIndex >= 2) ? 0x80 : 0x00) | (dayMode ? 0x40 : 0x00) | (dayMode ? (dayDate & 0x7) : (ByteToBCD(dayDate) & 0x3F)))
            };
            if (aSecBox != null) {
                buf = [
                    (byte)(((aModeBox.SelectedIndex == 5) ? 0x80 : 0x00) | ByteToBCD(sec) & 0x7F),
                    ..buf
                ];
            }

            if (!I2CDriverLib.I2C_WriteReg(ref App.i2cd, RTC_ADDR, (byte)((index == 1) ? 0x07 : 0x0B), buf)) {
                DisconnectReset();
                return;
            }

            TimerTick();
        }

        private void DoROMRead(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            if (!ushort.TryParse(romAddrBox.Text, System.Globalization.NumberStyles.HexNumber, null, out var addr) || addr > 0xFFF) {
                MessageBox.Show("Address must be in [0x000, 0xFFF]!");
                return;
            }

            var addrBuf = new byte[2] { (byte)(addr >> 8), (byte)(addr & 0xFF) };
            var readBuf = new byte[1];

            int r = I2CDriverLib.i2c_start(ref App.i2cd, EEPROM_ADDR, 0);
            if (r == 0) {
                I2CDriverLib.i2c_stop(ref App.i2cd);
                I2CDriverLib.i2c_getstatus(ref App.i2cd);
                romDataBox.Text = "START ERR";
                return;
            }

            r = I2CDriverLib.i2c_write(ref App.i2cd, addrBuf, 2);
            if (r == 0) {
                I2CDriverLib.i2c_stop(ref App.i2cd);
                I2CDriverLib.i2c_getstatus(ref App.i2cd);
                romDataBox.Text = "ADDR WRITE ERR";
                return;
            }

            r = I2CDriverLib.i2c_start(ref App.i2cd, EEPROM_ADDR, 1);
            if (r == 0) {
                I2CDriverLib.i2c_stop(ref App.i2cd);
                I2CDriverLib.i2c_getstatus(ref App.i2cd);
                romDataBox.Text = "RESTART ERR";
                return;
            }

            I2CDriverLib.i2c_read(ref App.i2cd, readBuf, 1);
            I2CDriverLib.i2c_stop(ref App.i2cd);
            I2CDriverLib.i2c_getstatus(ref App.i2cd);

            romDataBox.Text = readBuf[0].ToString("X2");
        }

        private void DoROMWrite(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            if (!ushort.TryParse(romAddrBox.Text, System.Globalization.NumberStyles.HexNumber, null, out var addr) || addr > 0xFFF) {
                MessageBox.Show("Address must be in [0x000, 0xFFF]!");
                return;
            }

            if (!byte.TryParse(romDataBox.Text, System.Globalization.NumberStyles.HexNumber, null, out var data)) {
                MessageBox.Show("Data must be in [0x00, 0xFF]!");
                return;
            }

            var writeBuf = new byte[3] { (byte)(addr >> 8), (byte)(addr & 0xFF), data };

            int r = I2CDriverLib.i2c_start(ref App.i2cd, EEPROM_ADDR, 0);
            if (r == 0) {
                I2CDriverLib.i2c_stop(ref App.i2cd);
                I2CDriverLib.i2c_getstatus(ref App.i2cd);
                romDataBox.Text = "START ERR";
                return;
            }

            r = I2CDriverLib.i2c_write(ref App.i2cd, writeBuf, 3);
            I2CDriverLib.i2c_stop(ref App.i2cd);
            I2CDriverLib.i2c_getstatus(ref App.i2cd);
            if (r == 0) {
                romDataBox.Text = "WRITE ERR";
                return;
            }
        }

        private void DoROMBatchWrite(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            if (!ushort.TryParse(romAddrBox.Text, System.Globalization.NumberStyles.HexNumber, null, out var addr) || addr > 0xFFF) {
                MessageBox.Show("Address must be in [0x000, 0xFFF]!");
                return;
            }

            var writeBuf = new byte[34];
            writeBuf[0] = 0;
            writeBuf[1] = 0;
            for (byte i = 0; i < 32; i++) {
                writeBuf[i + 2] = i;
            }

            int r = I2CDriverLib.i2c_start(ref App.i2cd, EEPROM_ADDR, 0);
            if (r == 0) {
                I2CDriverLib.i2c_stop(ref App.i2cd);
                I2CDriverLib.i2c_getstatus(ref App.i2cd);
                romDataBox.Text = "START ERR";
                return;
            }

            r = I2CDriverLib.i2c_write(ref App.i2cd, writeBuf, 34);
            I2CDriverLib.i2c_stop(ref App.i2cd);
            I2CDriverLib.i2c_getstatus(ref App.i2cd);
            if (r == 0) {
                romDataBox.Text = "BATCH W ERR";
                return;
            }
        }

        private void DoROMBatchRead(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            if (!ushort.TryParse(romAddrBox.Text, System.Globalization.NumberStyles.HexNumber, null, out var addr) || addr > 0xFFF) {
                MessageBox.Show("Address must be in [0x000, 0xFFF]!");
                return;
            }

            var addrBuf = new byte[2] { (byte)(addr >> 8), (byte)(addr & 0xFF) };
            var readBuf = new byte[32];

            int r = I2CDriverLib.i2c_start(ref App.i2cd, EEPROM_ADDR, 0);
            if (r == 0) {
                I2CDriverLib.i2c_stop(ref App.i2cd);
                I2CDriverLib.i2c_getstatus(ref App.i2cd);
                romBatchRBox.Text = "START ERR";
                return;
            }

            r = I2CDriverLib.i2c_write(ref App.i2cd, addrBuf, 2);
            if (r == 0) {
                I2CDriverLib.i2c_stop(ref App.i2cd);
                I2CDriverLib.i2c_getstatus(ref App.i2cd);
                romBatchRBox.Text = "ADDR WRITE ERR";
                return;
            }

            r = I2CDriverLib.i2c_start(ref App.i2cd, EEPROM_ADDR, 1);
            if (r == 0) {
                I2CDriverLib.i2c_stop(ref App.i2cd);
                I2CDriverLib.i2c_getstatus(ref App.i2cd);
                romBatchRBox.Text = "RESTART ERR";
                return;
            }

            I2CDriverLib.i2c_read(ref App.i2cd, readBuf, 32);
            I2CDriverLib.i2c_stop(ref App.i2cd);
            I2CDriverLib.i2c_getstatus(ref App.i2cd);

            string res = readBuf[0].ToString("X2");
            for (int i = 1; i < 32; i++) {
                res += " " + readBuf[i].ToString("X2");
            }
            romBatchRBox.Text = res;
        }
    }
}
