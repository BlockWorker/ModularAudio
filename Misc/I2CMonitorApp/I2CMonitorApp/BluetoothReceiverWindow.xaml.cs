using SerialPortLib;
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
    /// Interaction logic for BluetoothReceiverWindow.xaml
    /// </summary>
    public partial class BluetoothReceiverWindow : Window {

        private const byte DEVICE_ID_CORRECT = 0xB2;
        private readonly byte[] NOTIF_MASK_DEFAULT_WRITE = [0xFF, 0x01];

        private readonly SerialPortInput port = new();
        private readonly Queue<byte> readBuffer = [];
        private readonly List<byte> parseBuffer = [];
        private bool readEscaping = false;
        private bool readWaitForStart = true;

        private readonly Timer timer;

        public BluetoothReceiverWindow() {
            InitializeComponent();

            port.MessageReceived += DataReceivedHandler;
            port.ConnectionStatusChanged += delegate (object sender, ConnectionStatusChangedEventArgs e) {
                if (!e.Connected) {
                    DisconnectReset();
                }
            };

            timer = new(OnTimerExpired);
            DisconnectReset();
            connPortBox.ItemsSource = SerialPort.GetPortNames();
        }

        void OnTimerExpired(object? state) {
            Dispatcher.Invoke(TimerTick);
        }

        private void TimerTick() {
            if (!port.IsConnected) {
                DisconnectReset();
                return;
            }

            DoReadRegister(0xFE);
            DoReadRegister(0x20);
            DoReadRegister(0x00);
        }

        private void WriteSerialData(byte[] data) {
            if (!port.IsConnected) {
                return;
            }

            List<byte> buf = [];
            buf.Add(0xF1);
            foreach (var b in data) {
                if (b == 0xF1 || b == 0xFA || b == 0xFF) {
                    buf.Add(0xFF); //escape
                }
                buf.Add(b);
            }
            buf.Add(0xFA);

            port.SendMessage(buf.ToArray());
        }

        private void ProcessReadData() {
            while (readBuffer.TryDequeue(out var b)) {
                if (readEscaping) {
                    if (b == 0xF1 || b == 0xFA || b == 0xFF) {
                        if (!readWaitForStart) {
                            parseBuffer.Add(b);
                        }
                    } else {
                        //invalid escape
                        parseBuffer.Clear();
                        readWaitForStart = true;
                    }
                    readEscaping = false;
                } else {
                    if (b == 0xFF) {
                        readEscaping = true;
                    } else if (readWaitForStart) {
                        if (b == 0xF1) {
                            readWaitForStart = false;
                            parseBuffer.Clear();
                        }
                    } else {
                        if (b == 0xF1) {
                            //unexpected start
                            parseBuffer.Clear();
                        } else if (b == 0xFA) {
                            var arr = parseBuffer.ToArray();
                            Dispatcher.Invoke(() => HandleReceivedNotification(arr));
                            parseBuffer.Clear();
                            readWaitForStart = true;
                        } else {
                            parseBuffer.Add(b);
                        }
                    }
                }
            }
        }

        private void DataReceivedHandler(object sender, MessageReceivedEventArgs e) {
            if (!port.IsConnected) {
                return;
            }

            foreach (var b in e.Data) {
                readBuffer.Enqueue(b);
            }

            ProcessReadData();
        }

        private void DisconnectReset() {
            port.Disconnect();

            timer.Change(Timeout.Infinite, Timeout.Infinite);

            readBuffer.Clear();
            parseBuffer.Clear();
            readEscaping = false;
            readWaitForStart = true;

            connLabel.Content = "Disconnected";
            idLabel.Content = "0x00";
            connPortBox.IsEnabled = true;
            connButton.Content = "Connect";
        }

        private void OnConnectButtonClick(object sender, RoutedEventArgs e) {
            if (!port.IsConnected) {
                try {
                    if (connPortBox.SelectedIndex < 0) {
                        MessageBox.Show("Please select a port!");
                        return;
                    }

                    port.SetPort((string)connPortBox.SelectedValue, 115200, StopBits.One, Parity.None, DataBits.Eight);

                    readBuffer.Clear();
                    parseBuffer.Clear();
                    readEscaping = false;
                    readWaitForStart = true;

                    eventBox.Items.Clear();

                    if (!port.Connect()) {
                        MessageBox.Show("Connection failed...");
                        DisconnectReset();
                        return;
                    }

                    ReadAndInitRegisters();

                    timer.Change(2000, 2000);

                    connLabel.Content = "Connected";
                    idLabel.Content = "0x??";
                    connPortBox.IsEnabled = false;
                    connButton.Content = "Disconnect";
                } catch {
                    DisconnectReset();
                    MessageBox.Show("Connection failed...");
                }
            } else {
                DisconnectReset();
            }
        }

        private void DoReadRegister(byte address) {
            byte[] buf = [0x00, address];
            WriteSerialData(buf);
        }

        private void DoWriteRegister(byte address, byte[] data) {
            byte[] buf = new byte[data.Length + 2];
            buf[0] = 0x01;
            buf[1] = address;
            data.CopyTo(buf, 2);
            WriteSerialData(buf);
        }

        private void ReadAndInitRegisters() {
            DoWriteRegister(0x20, NOTIF_MASK_DEFAULT_WRITE);
            DoReadRegister(0x20);
            DoReadRegister(0xFE);
            DoReadRegister(0x00);
            DoReadRegister(0x01);
            DoReadRegister(0x02);
            DoReadRegister(0x03);
            DoReadRegister(0x04);
            DoReadRegister(0x05);
            DoReadRegister(0x06);
            DoReadRegister(0x07);
            DoReadRegister(0x08);
        }

        private void HandleReceivedNotification(byte[] data) {
            if (data.Length < 2) return;
            switch (data[0]) {
                case 0x00:
                    //event
                    switch (data[1]) {
                        case 0x00:
                            eventBox.Items.Add("MCU Reset");
                            //ReadAndInitRegisters();
                            break;
                        case 0x01:
                            if (data.Length < 3) return;
                            eventBox.Items.Add($"Write ACK 0x{data[2]:X2}");
                            break;
                        case 0x02:
                            if (data.Length < 4) return;
                            eventBox.Items.Add($"Error 0x{data[3]:X2}{data[2]:X2}");
                            break;
                        case 0x03:
                            eventBox.Items.Add("BT Module Reset");
                            ReadAndInitRegisters();
                            break;
                        default:
                            eventBox.Items.Add($"Unknown event 0x{data[1]:X2}");
                            break;
                    }
                    break;
                case 0x01:
                case 0x02:
                    //data
                    switch (data[1]) {
                        case 0x00:
                            if (data.Length < 4) return;
                            statusLowField.Value = data[2];
                            if (data[0] == 0x01) {
                                statusHighField.Value = (byte)((statusHighField.Value & 0x80) | data[3]);
                            } else {
                                statusHighField.Value = data[3];
                            }
                            break;
                        case 0x01:
                            if (data.Length < 3) return;
                            if (!volumeBox.IsFocused) {
                                volumeBox.Text = data[2].ToString();
                            }
                            break;
                        case 0x02:
                            if (data.Length < 3) return;
                            titleBox.Text = Encoding.UTF8.GetString(data.Skip(2).TakeWhile(b => b != 0).ToArray());
                            break;
                        case 0x03:
                            if (data.Length < 3) return;
                            artistBox.Text = Encoding.UTF8.GetString(data.Skip(2).TakeWhile(b => b != 0).ToArray());
                            break;
                        case 0x04:
                            if (data.Length < 3) return;
                            albumBox.Text = Encoding.UTF8.GetString(data.Skip(2).TakeWhile(b => b != 0).ToArray());
                            break;
                        case 0x05:
                            if (data.Length < 8) return;
                            devAddrBox.Text = BitConverter.ToUInt64([.. data, 0, 0], 2).ToString("X12");
                            break;
                        case 0x06:
                            if (data.Length < 3) return;
                            devNameBox.Text = Encoding.UTF8.GetString(data.Skip(2).TakeWhile(b => b != 0).ToArray());
                            break;
                        case 0x07:
                            if (data.Length < 6) return;
                            rssiBox.Text = BitConverter.ToInt16(data, 2).ToString();
                            qualityBox.Text = BitConverter.ToUInt16(data, 4).ToString();
                            break;
                        case 0x08:
                            if (data.Length < 3) return;
                            codecBox.Text = Encoding.UTF8.GetString(data.Skip(2).TakeWhile(b => b != 0).ToArray());
                            break;
                        case 0x20:
                            if (data.Length < 4) return;
                            notifMaskLowField.Value = data[2];
                            notifMaskHighField.Value = data[3];
                            break;
                        case 0xFE:
                            if (data.Length < 3) return;
                            idLabel.Content = $"0x{data[2]:X2}";
                            break;
                        default:
                            eventBox.Items.Add($"Unknown register address 0x{data[1]:X2}");
                            break;
                    }
                    break;
                default:
                    eventBox.Items.Add($"Unknown notification 0x{data[0]:X2}");
                    break;
            }
        }

        private void DoControlApply(object sender, RoutedEventArgs e) {
            if (!port.IsConnected) {
                return;
            }

            byte[] data = [controlField.SwitchedValue];
            DoWriteRegister(0x30, data);

            controlField.ResetSwitches();
        }

        private void DoNotifMaskApply(object sender, RoutedEventArgs e) {
            if (!port.IsConnected) {
                return;
            }

            byte[] data = [notifMaskLowField.SwitchedValue, notifMaskHighField.SwitchedValue];
            DoWriteRegister(0x20, data);

            DoReadRegister(0x20);
        }

        private void DoConnControlApply(object sender, RoutedEventArgs e) {
            if (!port.IsConnected) {
                return;
            }

            byte[] data = [connControlField.SwitchedValue];
            DoWriteRegister(0x31, data);

            connControlField.ResetSwitches();
        }

        private void DoMediaControlSend(object sender, RoutedEventArgs e) {
            if (!port.IsConnected) {
                return;
            }

            if (mediaControlBox.SelectedIndex < 0 || mediaControlBox.SelectedIndex > 8) {
                MessageBox.Show("Please select a media command first!");
                return;
            }

            byte[] data = [(byte)(mediaControlBox.SelectedIndex + 1)];
            DoWriteRegister(0x32, data);
        }

        private void DoTonePlay(object sender, RoutedEventArgs e) {
            if (!port.IsConnected) {
                return;
            }

            byte[] strBytes = [.. Encoding.ASCII.GetBytes(toneBox.Text), 0];

            if (strBytes.Length >= 240) {
                MessageBox.Show("Tone string too long! (240 bytes max)");
                return;
            }

            DoWriteRegister(0x33, strBytes);
        }

        private void DoVolumeApply(object sender, RoutedEventArgs e) {
            if (!port.IsConnected) {
                return;
            }

            if (!byte.TryParse(volumeBox.Text, out var vol) || vol > 127) {
                MessageBox.Show("Please enter a valid volume! (0-127)");
                return;
            }

            byte[] data = [vol];
            DoWriteRegister(0x01, data);
        }

        private void Window_Closing(object sender, System.ComponentModel.CancelEventArgs e) {
            DisconnectReset();
        }
    }
}
