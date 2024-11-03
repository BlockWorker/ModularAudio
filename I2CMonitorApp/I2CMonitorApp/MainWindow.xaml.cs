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

        private Timer timer;
        private bool readAll;

        private I2CDriverLib.I2CDriver d;

        public MainWindow() {
            InitializeComponent();
            timer = new(OnTimerExpired);
            readAll = true;
            d = new();
            connPortBox.ItemsSource = SerialPort.GetPortNames();
        }

        void OnTimerExpired(object? state) {

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
                    idLabel.Content = $"0x{id[0]:02X}";
                } else {
                    idLabel.Content = "ERR";
                }
            } else {
                I2CDriverLib.i2c_disconnect(ref d);

                connLabel.Content = "Disconnected";
                idLabel.Content = "0x00";
                connPortBox.IsEnabled = true;
                connButton.Content = "Connect";
            }
        }
    }
}