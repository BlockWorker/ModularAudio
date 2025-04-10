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
    /// Interaction logic for DAPWindow.xaml
    /// </summary>
    public partial class DAPWindow : Window {

        private const byte DEV_ADDR = 0x4A;
        private const int READ_ALL_PERIOD = 5;
        private const byte DEVICE_ID_CORRECT = 0xD4;

        private bool i2c_connected = false;
        private readonly Timer timer;
        private int readAllCycles;

        public DAPWindow() {
            InitializeComponent();
            timer = new(OnTimerExpired);
            readAllCycles = READ_ALL_PERIOD;
            connPortBox.ItemsSource = SerialPort.GetPortNames();
            connPortBox.Text = App.i2c_portname;
        }

        private float Q31ToFloat(int q31) {
            return q31 / (float)0x80000000UL;
        }

        private int FloatToQ31(float f) {
            if (f < -1.0f || f >= 1.0f) throw new ArgumentException("Q31 only supports values in the range [-1, 1)");
            return (int)Math.Floor(f * 0x80000000UL);
        }

        void OnTimerExpired(object? state) {
            Dispatcher.Invoke(TimerTick);
        }

        private void TimerTick() {
            if (!i2c_connected || App.i2cd.connected == 0) {
                DisconnectReset();
                return;
            }

            byte[] buf = new byte[256];

            if (!I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0xFF, 1, ref buf)) {
                DisconnectReset();
                return;
            }
            if (buf[0] != DEVICE_ID_CORRECT) {
                DisconnectReset();
                return;
            }

            if (!I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0x01, 1, ref buf)) {
                DisconnectReset();
                return;
            }
            statusField.Value = buf[0];

            if (!I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0x11, 1, ref buf)) {
                DisconnectReset();
                return;
            }
            intFlagsField.Value = buf[0];

            if (!I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0x20, 2, ref buf)) {
                DisconnectReset();
                return;
            }
            if (!inputBox.IsFocused) {
                if (buf[0] > 0 && buf[0] < 6) {
                    inputBox.SelectedIndex = buf[0] - 1;
                } else {
                    inputBox.SelectedIndex = -1;
                }
            }
            availableInputField.Value = buf[1];

            if (!I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0x30, 12, ref buf)) {
                DisconnectReset();
                return;
            }
            srcInputRateBox.Text = BitConverter.ToUInt32(buf, 0).ToString();
            srcRateErrorBox.Text = BitConverter.ToSingle(buf, 4).ToString("P3");
            srcBufferErrorBox.Text = BitConverter.ToSingle(buf, 8).ToString("F1");


            if (readAllCycles++ < READ_ALL_PERIOD) return;

            readAllCycles = 0;

            if (!I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0x08, 1, ref buf)) {
                DisconnectReset();
                return;
            }
            controlField.Value = buf[0];

            if (!I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0x10, 1, ref buf)) {
                DisconnectReset();
                return;
            }
            intMaskField.Value = buf[0];

            if (!I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0x28, 12, ref buf)) {
                DisconnectReset();
                return;
            }
            if (!i2s1SampleRateBox.IsFocused) {
                i2s1SampleRateBox.Text = BitConverter.ToUInt32(buf, 0).ToString();
            }
            if (!i2s2SampleRateBox.IsFocused) {
                i2s2SampleRateBox.Text = BitConverter.ToUInt32(buf, 4).ToString();
            }
            if (!i2s3SampleRateBox.IsFocused) {
                i2s3SampleRateBox.Text = BitConverter.ToUInt32(buf, 8).ToString();
            }

            if (!I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0x40, 40, ref buf)) {
                DisconnectReset();
                return;
            }
            if (!mixerIn1Out1Box.IsFocused && !mixerIn1Out2Box.IsFocused && !mixerIn2Out1Box.IsFocused && !mixerIn2Out2Box.IsFocused) {
                mixerIn1Out1Box.Text = (Q31ToFloat(BitConverter.ToInt32(buf, 0)) * 2.0f).ToString("F3");
                mixerIn2Out1Box.Text = (Q31ToFloat(BitConverter.ToInt32(buf, 4)) * 2.0f).ToString("F3");
                mixerIn1Out2Box.Text = (Q31ToFloat(BitConverter.ToInt32(buf, 8)) * 2.0f).ToString("F3");
                mixerIn2Out2Box.Text = (Q31ToFloat(BitConverter.ToInt32(buf, 12)) * 2.0f).ToString("F3");
            }
            if (!volume1Box.IsFocused && !volume2Box.IsFocused) {
                volume1Box.Text = BitConverter.ToSingle(buf, 16).ToString("F1");
                volume2Box.Text = BitConverter.ToSingle(buf, 20).ToString("F1");
            }
            if (!loudness1Box.IsFocused && !loudness2Box.IsFocused) {
                loudness1Box.Text = BitConverter.ToSingle(buf, 24).ToString("F1");
                loudness2Box.Text = BitConverter.ToSingle(buf, 28).ToString("F1");
            }
            if (!biquadCount1Box.IsFocused && !biquadCount2Box.IsFocused && !biquadShift1Box.IsFocused && !biquadShift2Box.IsFocused) {
                biquadCount1Box.Text = buf[32].ToString();
                biquadCount2Box.Text = buf[33].ToString();
                biquadShift1Box.Text = buf[34].ToString();
                biquadShift2Box.Text = buf[35].ToString();
            }
            if (!firLength1Box.IsFocused && !firLength2Box.IsFocused) {
                firLength1Box.Text = BitConverter.ToUInt16(buf, 36).ToString();
                firLength2Box.Text = BitConverter.ToUInt16(buf, 38).ToString();
            }
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
                if (I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0xFF, 1, ref id)) {
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

            if (!I2CDriverLib.I2C_WriteReg(ref App.i2cd, DEV_ADDR, 0x08, buf)) {
                DisconnectReset();
                return;
            }

            controlField.ResetSwitches();

            if (I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0x08, 1, ref buf)) {
                controlField.Value = buf[0];
            }
        }

        private void DoIntMaskApply(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            var buf = new byte[1] { intMaskField.SwitchedValue };

            if (!I2CDriverLib.I2C_WriteReg(ref App.i2cd, DEV_ADDR, 0x10, buf)) {
                DisconnectReset();
                return;
            }

            intMaskField.ResetSwitches();

            if (I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0x10, 1, ref buf)) {
                intMaskField.Value = buf[0];
            }
        }

        private void DoIntFlagsClear(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            var buf = new byte[1] { 0 };

            if (!I2CDriverLib.I2C_WriteReg(ref App.i2cd, DEV_ADDR, 0x11, buf)) {
                DisconnectReset();
                return;
            }

            if (I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0x11, 1, ref buf)) {
                intFlagsField.Value = buf[0];
            }
        }

        private void DoInputApply(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            if (inputBox.SelectedIndex < 0 || inputBox.SelectedIndex > 4) {
                MessageBox.Show("Invalid input selected");
                return;
            }

            var buf = new byte[1] { (byte)(inputBox.SelectedIndex + 1) };

            if (!I2CDriverLib.I2C_WriteReg(ref App.i2cd, DEV_ADDR, 0x20, buf)) {
                DisconnectReset();
                return;
            }

            if (I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0x20, 1, ref buf)) {
                if (buf[0] > 0 && buf[0] < 6) {
                    inputBox.SelectedIndex = buf[0] - 1;
                } else {
                    inputBox.SelectedIndex = -1;
                }
            }
        }

        private void DoI2SRateApply(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            TextBox box;
            byte addr;
            switch((string)((Button)sender).Tag) {
                case "1":
                    box = i2s1SampleRateBox;
                    addr = 0x28;
                    break;
                case "2":
                    box = i2s2SampleRateBox;
                    addr = 0x29;
                    break;
                case "3":
                    box = i2s3SampleRateBox;
                    addr = 0x2A;
                    break;
                default:
                    return;
            }

            if (!uint.TryParse(box.Text, out var rate) || (rate != 44100 && rate != 48000 && rate != 96000)) {
                MessageBox.Show("Invalid input rate - only 44.1K, 48K, 96K are accepted");
                return;
            }

            byte[] buf = BitConverter.GetBytes(rate);

            if (!I2CDriverLib.I2C_WriteReg(ref App.i2cd, DEV_ADDR, addr, buf)) {
                DisconnectReset();
                return;
            }

            if (I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, addr, 4, ref buf)) {
                box.Text = BitConverter.ToUInt32(buf, 0).ToString();
            }
        }

        private void DoMixerApply(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            if (!float.TryParse(mixerIn1Out1Box.Text, out var i1o1) || !float.TryParse(mixerIn2Out1Box.Text, out var i2o1) ||
                !float.TryParse(mixerIn1Out2Box.Text, out var i1o2) || !float.TryParse(mixerIn2Out2Box.Text, out var i2o2) ||
                i1o1 < -2.0f || i1o1 >= 2.0f || i2o1 < -2.0f || i2o1 >= 2.0f || i1o2 < -2.0f || i1o2 >= 2.0f || i2o2 < -2.0f || i2o2 >= 2.0f) {
                MessageBox.Show("Invalid mixer gains - must all be in range [-2, 2)");
                return;
            }

            var qi1o1 = FloatToQ31(i1o1 / 2.0f);
            var qi2o1 = FloatToQ31(i2o1 / 2.0f);
            var qi1o2 = FloatToQ31(i1o2 / 2.0f);
            var qi2o2 = FloatToQ31(i2o2 / 2.0f);

            byte[] buf = [..BitConverter.GetBytes(qi1o1), ..BitConverter.GetBytes(qi2o1), .. BitConverter.GetBytes(qi1o2), .. BitConverter.GetBytes(qi2o2)];

            if (!I2CDriverLib.I2C_WriteReg(ref App.i2cd, DEV_ADDR, 0x40, buf)) {
                DisconnectReset();
                return;
            }

            if (I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0x40, 16, ref buf)) {
                mixerIn1Out1Box.Text = (Q31ToFloat(BitConverter.ToInt32(buf, 0)) * 2.0f).ToString("F3");
                mixerIn2Out1Box.Text = (Q31ToFloat(BitConverter.ToInt32(buf, 4)) * 2.0f).ToString("F3");
                mixerIn1Out2Box.Text = (Q31ToFloat(BitConverter.ToInt32(buf, 8)) * 2.0f).ToString("F3");
                mixerIn2Out2Box.Text = (Q31ToFloat(BitConverter.ToInt32(buf, 12)) * 2.0f).ToString("F3");
            }
        }

        private void DoVolumeApply(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            if (!float.TryParse(volume1Box.Text, out var vol1) || !float.TryParse(volume2Box.Text, out var vol2) ||
                vol1 < -120.0f || vol1 > 20.0f || vol2 < -120.0f || vol2 > 20.0f) {
                MessageBox.Show("Invalid volumes - must be in range [-120, 20]");
                return;
            }

            byte[] buf = [.. BitConverter.GetBytes(vol1), .. BitConverter.GetBytes(vol2)];

            if (!I2CDriverLib.I2C_WriteReg(ref App.i2cd, DEV_ADDR, 0x41, buf)) {
                DisconnectReset();
                return;
            }

            if (I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0x41, 8, ref buf)) {
                volume1Box.Text = BitConverter.ToSingle(buf, 0).ToString("F1");
                volume2Box.Text = BitConverter.ToSingle(buf, 4).ToString("F1");
            }
        }

        private void DoLoudnessApply(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            if (!float.TryParse(loudness1Box.Text, out var loudness1) || !float.TryParse(loudness2Box.Text, out var loudness2) || loudness1 > 0.0f || loudness2 > 0.0f) {
                MessageBox.Show("Invalid loudness gains - must be 0 or less");
                return;
            }

            byte[] buf = [.. BitConverter.GetBytes(loudness1), .. BitConverter.GetBytes(loudness2)];

            if (!I2CDriverLib.I2C_WriteReg(ref App.i2cd, DEV_ADDR, 0x42, buf)) {
                DisconnectReset();
                return;
            }

            if (I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0x42, 8, ref buf)) {
                loudness1Box.Text = BitConverter.ToSingle(buf, 0).ToString("F1");
                loudness2Box.Text = BitConverter.ToSingle(buf, 4).ToString("F1");
            }
        }

        private void DoBiquadApply(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            if (!byte.TryParse(biquadCount1Box.Text, out var count1) || !byte.TryParse(biquadCount2Box.Text, out var count2) ||
                !byte.TryParse(biquadShift1Box.Text, out var shift1) || !byte.TryParse(biquadShift2Box.Text, out var shift2) ||
                count1 > 16 || count2 > 16 || shift1 > 31 || shift2 > 31) {
                MessageBox.Show("Invalid biquad setup - counts must be 0-16, shifts must be 0-31");
                return;
            }

            var buf = new byte[4] { count1, count2, shift1, shift2 };

            if (!I2CDriverLib.I2C_WriteReg(ref App.i2cd, DEV_ADDR, 0x43, buf)) {
                DisconnectReset();
                return;
            }

            if (I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0x43, 4, ref buf)) {
                biquadCount1Box.Text = buf[0].ToString();
                biquadCount2Box.Text = buf[1].ToString();
                biquadShift1Box.Text = buf[2].ToString();
                biquadShift2Box.Text = buf[3].ToString();
            }
        }

        private void DoFIRApply(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            if (!ushort.TryParse(firLength1Box.Text, out var length1) || !ushort.TryParse(firLength2Box.Text, out var length2) || length1 > 300 || length2 > 300) {
                MessageBox.Show("Invalid FIR filter lengths - must be 300 or less");
                return;
            }

            byte[] buf = [.. BitConverter.GetBytes(length1), .. BitConverter.GetBytes(length2)];

            if (!I2CDriverLib.I2C_WriteReg(ref App.i2cd, DEV_ADDR, 0x44, buf)) {
                DisconnectReset();
                return;
            }

            if (I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0x44, 4, ref buf)) {
                firLength1Box.Text = BitConverter.ToUInt16(buf, 0).ToString();
                firLength2Box.Text = BitConverter.ToUInt16(buf, 2).ToString();
            }
        }

        private void DoBiquadLPSend(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            byte addr;
            switch ((string)((Button)sender).Tag) {
                case "1":
                    addr = 0x50;
                    break;
                case "2":
                    addr = 0x51;
                    break;
                default:
                    return;
            }

            float[] coeffs = [0.0003750695191391371f, 0.0007501390382782742f, 0.0003750695191391371f, 1.9444771540706607f, -0.9459774321472173f];
            byte[] buf = new byte[320];
            for (int i = 0; i < 16; i++) {
                for (int j = 0; j < 5; j++) {
                    Array.Copy(BitConverter.GetBytes(FloatToQ31(coeffs[j] / 2.0f)), 0, buf, (5 * i + j) * 4, 4);
                }
            }

            if (!I2CDriverLib.I2C_WriteReg(ref App.i2cd, DEV_ADDR, addr, buf)) {
                DisconnectReset();
                return;
            }
        }

        private void DoBiquadHPSend(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            byte addr;
            switch ((string)((Button)sender).Tag) {
                case "1":
                    addr = 0x50;
                    break;
                case "2":
                    addr = 0x51;
                    break;
                default:
                    return;
            }

            float[] coeffs = [0.9726136465544695f, -1.945227293108939f, 0.9726136465544695f, 1.9444771540706607f, -0.9459774321472173f];
            byte[] buf = new byte[320];
            for (int i = 0; i < 16; i++) {
                for (int j = 0; j < 5; j++) {
                    Array.Copy(BitConverter.GetBytes(FloatToQ31(coeffs[j] / 2.0f)), 0, buf, (5 * i + j) * 4, 4);
                }
            }

            if (!I2CDriverLib.I2C_WriteReg(ref App.i2cd, DEV_ADDR, addr, buf)) {
                DisconnectReset();
                return;
            }
        }

        private void DoFIR1Send(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            byte addr;
            switch ((string)((Button)sender).Tag) {
                case "1":
                    addr = 0x58;
                    break;
                case "2":
                    addr = 0x59;
                    break;
                default:
                    return;
            }

            byte[] buf = new byte[1200];
            Array.Fill<byte>(buf, 0);
            buf[0] = 1;
            Array.Copy(BitConverter.GetBytes(int.MaxValue), 0, buf, 1196, 4);

            if (!I2CDriverLib.I2C_WriteReg(ref App.i2cd, DEV_ADDR, addr, buf)) {
                DisconnectReset();
                return;
            }
        }

        private void DoFIR2Send(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            byte addr;
            switch ((string)((Button)sender).Tag) {
                case "1":
                    addr = 0x58;
                    break;
                case "2":
                    addr = 0x59;
                    break;
                default:
                    return;
            }

            byte[] buf = new byte[10];
            Array.Fill<byte>(buf, 0);
            buf[0] = 1;

            if (!I2CDriverLib.I2C_WriteReg(ref App.i2cd, DEV_ADDR, addr, buf)) {
                DisconnectReset();
                return;
            }
        }

        private void DoBiquadRead(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            var buf = new byte[240];

            if (I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0x50, 240, ref buf)) {
                var b00 = Q31ToFloat(BitConverter.ToInt32(buf, 0));
                var b01 = Q31ToFloat(BitConverter.ToInt32(buf, 4));
                var b02 = Q31ToFloat(BitConverter.ToInt32(buf, 8));
                var a01 = Q31ToFloat(BitConverter.ToInt32(buf, 12));
                var a02 = Q31ToFloat(BitConverter.ToInt32(buf, 16));
                var bB0 = Q31ToFloat(BitConverter.ToInt32(buf, 220));
                var bB1 = Q31ToFloat(BitConverter.ToInt32(buf, 224));
                var bB2 = Q31ToFloat(BitConverter.ToInt32(buf, 228));
                var aB1 = Q31ToFloat(BitConverter.ToInt32(buf, 232));
                var aB2 = Q31ToFloat(BitConverter.ToInt32(buf, 236));
                MessageBox.Show($"Ch1 Biquad: {b00} {b01} {b02} {a01} {a02} ... {bB0} {bB1} {bB2} {aB1} {aB2}");
            } else {
                MessageBox.Show("Ch1 Biquad read failed!");
            }

            if (I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0x51, 240, ref buf)) {
                var b00 = Q31ToFloat(BitConverter.ToInt32(buf, 0));
                var b01 = Q31ToFloat(BitConverter.ToInt32(buf, 4));
                var b02 = Q31ToFloat(BitConverter.ToInt32(buf, 8));
                var a01 = Q31ToFloat(BitConverter.ToInt32(buf, 12));
                var a02 = Q31ToFloat(BitConverter.ToInt32(buf, 16));
                var bB0 = Q31ToFloat(BitConverter.ToInt32(buf, 220));
                var bB1 = Q31ToFloat(BitConverter.ToInt32(buf, 224));
                var bB2 = Q31ToFloat(BitConverter.ToInt32(buf, 228));
                var aB1 = Q31ToFloat(BitConverter.ToInt32(buf, 232));
                var aB2 = Q31ToFloat(BitConverter.ToInt32(buf, 236));
                MessageBox.Show($"Ch2 Biquad: {b00} {b01} {b02} {a01} {a02} ... {bB0} {bB1} {bB2} {aB1} {aB2}");
            } else {
                MessageBox.Show("Ch2 Biquad read failed!");
            }
        }

        private void DoFIRRead(object sender, RoutedEventArgs e) {
            if (!i2c_connected || App.i2cd.connected == 0) return;

            var buf = new byte[252];

            if (I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0x58, 252, ref buf)) {
                var c299 = Q31ToFloat(BitConverter.ToInt32(buf, 0));
                var c298 = Q31ToFloat(BitConverter.ToInt32(buf, 4));
                var c238 = Q31ToFloat(BitConverter.ToInt32(buf, 244));
                var c237 = Q31ToFloat(BitConverter.ToInt32(buf, 248));
                MessageBox.Show($"Ch1 FIR: ... {c237} {c238} ... {c298} {c299}");
            } else {
                MessageBox.Show("Ch1 FIR read failed!");
            }

            if (I2CDriverLib.I2C_ReadReg(ref App.i2cd, DEV_ADDR, 0x59, 252, ref buf)) {
                var c299 = Q31ToFloat(BitConverter.ToInt32(buf, 0));
                var c298 = Q31ToFloat(BitConverter.ToInt32(buf, 4));
                var c238 = Q31ToFloat(BitConverter.ToInt32(buf, 244));
                var c237 = Q31ToFloat(BitConverter.ToInt32(buf, 248));
                MessageBox.Show($"Ch2 FIR: ... {c237} {c238} ... {c298} {c299}");
            } else {
                MessageBox.Show("Ch2 FIR read failed!");
            }
        }
    }
}
