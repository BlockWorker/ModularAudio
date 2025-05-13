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
using SerialPortLib;

namespace I2CMonitorApp {
    /// <summary>
    /// Interaction logic for BatteryMonitorWindow.xaml
    /// </summary>
    public partial class BatteryMonitorWindow : Window {

        private const byte DEVICE_ID_CORRECT = 0xBD;
        private readonly byte[] NOTIF_MASK_DEFAULT_WRITE = [0xFF, 0x0F];

        private readonly SerialPortInput port = new();
        private readonly Queue<byte> readBuffer = [];
        private readonly List<byte> parseBuffer = [];
        private bool readEscaping = false;
        private bool readWaitForStart = true;

        private readonly Timer timer;

        private static readonly UARTCrc16 crc = new();

        public BatteryMonitorWindow() {
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
            DoReadRegister(0x30);
            DoReadRegister(0x00);
        }

        private void WriteSerialData(byte[] data) {
            if (!port.IsConnected) {
                return;
            }

            crc.Reset();
            var crcValue = crc.ComputeHash(data);

            List<byte> buf = [];
            buf.Add(0xF1); //add start byte
            //add data
            foreach (var b in data) {
                if (b == 0xF1 || b == 0xFA || b == 0xFF) {
                    buf.Add(0xFF); //escape
                }
                buf.Add(b);
            }
            //add CRC, in big endian order so that CRC(data..crc) = 0
            foreach (var b in crcValue.Reverse()) {
                if (b == 0xF1 || b == 0xFA || b == 0xFF) {
                    buf.Add(0xFF);
                }
                buf.Add(b);
            }
            buf.Add(0xFA); //add end byte

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

                    port.SetPort((string)connPortBox.SelectedValue, 57600, StopBits.One, Parity.None, DataBits.Eight);

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
            DoReadRegister(0x30);
            DoReadRegister(0x00);
            DoReadRegister(0x01);
            DoReadRegister(0x02);
            DoReadRegister(0x03);
            DoReadRegister(0x04);
            DoReadRegister(0x05);
            DoReadRegister(0x06);
            DoReadRegister(0x07);
            DoReadRegister(0x08);
            DoReadRegister(0x09);
            DoReadRegister(0x0A);
            DoReadRegister(0x0B);
        }

        private void HandleReceivedNotification(byte[] data) {
            if (data.Length < 4) return;

            crc.Reset();
            var crcValue = crc.ComputeHash(data);
            if (crcValue[0] != 0 || crcValue[1] != 0) {
                eventBox.Items.Add("CRC mismatch on received notification");
                return;
            }

            switch (data[0]) {
                case 0x00:
                    //event
                    switch (data[1]) {
                        case 0x00:
                            eventBox.Items.Add("MCU Reset");
                            ReadAndInitRegisters();
                            break;
                        case 0x01:
                            if (data.Length < 5) return;
                            eventBox.Items.Add($"Write ACK 0x{data[2]:X2}");
                            break;
                        case 0x02:
                            if (data.Length < 6) return;
                            eventBox.Items.Add($"Error 0x{data[3]:X2}{data[2]:X2}");
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
                            if (data.Length < 5) return;
                            if (data[0] == 0x01) {
                                statusField.Value = (byte)((statusField.Value & 0xE0) | data[2]);
                            } else {
                                statusField.Value = data[2];
                            }
                            break;
                        case 0x01:
                            if (data.Length < 6) return;
                            stackBox.Text = BitConverter.ToUInt16(data, 2).ToString();
                            break;
                        case 0x02:
                            if (data.Length < 12) return;
                            cell1Box.Text = BitConverter.ToInt16(data, 2).ToString();
                            cell2Box.Text = BitConverter.ToInt16(data, 4).ToString();
                            cell3Box.Text = BitConverter.ToInt16(data, 6).ToString();
                            cell4Box.Text = BitConverter.ToInt16(data, 8).ToString();
                            break;
                        case 0x03:
                            if (data.Length < 8) return;
                            currentBox.Text = BitConverter.ToInt32(data, 2).ToString();
                            break;
                        case 0x04:
                            if (data.Length < 9) return;
                            socPercentBox.Text = (BitConverter.ToSingle(data, 2) * 100.0f).ToString("F2");
                            switch (data[6]) {
                                case 0x01:
                                    socLevelBox.Text = "Volt Only";
                                    break;
                                case 0x02:
                                    socLevelBox.Text = "Est Ref";
                                    break;
                                case 0x03:
                                    socLevelBox.Text = "Full Ref";
                                    break;
                                default:
                                    eventBox.Items.Add($"Unknown SoC level 0x{data[6]:X2}");
                                    goto case 0x00;
                                case 0x00:
                                    socLevelBox.Text = "Invalid";
                                    break;
                            }
                            break;
                        case 0x05:
                            if (data.Length < 9) return;
                            socEnergyBox.Text = BitConverter.ToSingle(data, 2).ToString("F2");
                            switch (data[6]) {
                                case 0x01:
                                    socLevelBox.Text = "Volt Only";
                                    break;
                                case 0x02:
                                    socLevelBox.Text = "Est Ref";
                                    break;
                                case 0x03:
                                    socLevelBox.Text = "Full Ref";
                                    break;
                                default:
                                    eventBox.Items.Add($"Unknown SoC level 0x{data[6]:X2}");
                                    goto case 0x00;
                                case 0x00:
                                    socLevelBox.Text = "Invalid";
                                    break;
                            }
                            break;
                        case 0x06:
                            if (data.Length < 8) return;
                            if (!healthPercentBox.IsFocused) {
                                healthPercentBox.Text = (BitConverter.ToSingle(data, 2) * 100.0f).ToString("F2");
                            }
                            break;
                        case 0x07:
                            if (data.Length < 6) return;
                            batTempBox.Text = BitConverter.ToInt16(data, 2).ToString();
                            break;
                        case 0x08:
                            if (data.Length < 6) return;
                            intTempBox.Text = BitConverter.ToInt16(data, 2).ToString();
                            break;
                        case 0x09:
                            if (data.Length < 6) return;
                            alertsLowField.Value = data[2];
                            alertsHighField.Value = data[3];
                            break;
                        case 0x0A:
                            if (data.Length < 6) return;
                            faultsLowField.Value = data[2];
                            faultsHighField.Value = data[3];
                            break;
                        case 0x0B:
                            if (data.Length < 7) return;
                            switch(data[2]) {
                                case 0x00:
                                    shutdownTypeBox.Text = "None";
                                    break;
                                case 0x01:
                                    shutdownTypeBox.Text = "Full";
                                    break;
                                case 0x02:
                                    shutdownTypeBox.Text = "EoD";
                                    break;
                                case 0x03:
                                    shutdownTypeBox.Text = "User";
                                    break;
                                default:
                                    eventBox.Items.Add($"Unknown shutdown type 0x{data[2]:X2}");
                                    shutdownTypeBox.Text = "Unknown";
                                    break;
                            }
                            shutdownTimeBox.Text = BitConverter.ToUInt16(data, 3).ToString();
                            break;
                        case 0x20:
                            if (data.Length < 6) return;
                            notifMaskLowField.Value = data[2];
                            notifMaskHighField.Value = data[3];
                            break;
                        case 0x30:
                            if (data.Length < 5) return;
                            controlField.Value = data[2];
                            break;
                        case 0xFE:
                            if (data.Length < 5) return;
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

            DoReadRegister(0x30);
        }

        private void DoNotifMaskApply(object sender, RoutedEventArgs e) {
            if (!port.IsConnected) {
                return;
            }

            byte[] data = [notifMaskLowField.SwitchedValue, notifMaskHighField.SwitchedValue];
            DoWriteRegister(0x20, data);

            DoReadRegister(0x20);
        }

        private void DoHealthApply(object sender, RoutedEventArgs e) {
            if (!port.IsConnected) {
                return;
            }

            if (!float.TryParse(healthPercentBox.Text, out float health) || health < 0.1f || health > 1.0f) return;

            byte[] data = BitConverter.GetBytes(health);
            DoWriteRegister(0x06, data);

            DoReadRegister(0x06);
        }

        private void Window_Closing(object sender, System.ComponentModel.CancelEventArgs e) {
            DisconnectReset();
        }
    }
}
