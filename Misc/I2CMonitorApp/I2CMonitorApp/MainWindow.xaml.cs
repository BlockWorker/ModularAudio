using System;
using System.Collections.Generic;
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
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window {
        public MainWindow() {
            InitializeComponent();
        }

        private void OpenAmpWindow(object sender, RoutedEventArgs e) {
            new AmpWindow().Show();
        }

        private void OpenChargerWindow(object sender, RoutedEventArgs e) {
            new ChargerWindow().Show();
        }

        private void OpenBatteryWindow(object sender, RoutedEventArgs e) {
            new BatteryMonitorWindow().Show();
        }

        private void OpenBluetoothWindow(object sender, RoutedEventArgs e) {
            new BluetoothReceiverWindow().Show();
        }

        private void OpenDACWindow(object sender, RoutedEventArgs e) {
            new DACWindow().Show();
        }
    }
}
