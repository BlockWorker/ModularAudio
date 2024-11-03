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
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace I2CMonitorApp {
    /// <summary>
    /// Interaction logic for BitField.xaml
    /// </summary>
    public partial class BitField : UserControl {

        private readonly Color offColor = Colors.Transparent;

        private readonly Label[] _label_objects; 

        public string Labels {
            get => _labels.Aggregate((result, next) => result + '\n' + next).Trim();
            set {
                var temp_res = value.Split('\n', StringSplitOptions.None);
                if (temp_res.Length != 8) throw new ArgumentException("BitField needs exactly 8 labels.");
                _labels = temp_res;
                UpdateLabels();
            }
        }
        private string[] _labels = ["B0", "B1", "B2", "B3", "B4", "B5", "B6", "B7"];

        public byte Value {
            get => _value;
            set {
                _value = value;
                UpdateColors();
            }
        }
        private byte _value = 0;

        public byte SwitchedValue {
            get => (byte)(_value ^ _set_switches);
        }
        private byte _set_switches = 0;

        public bool AllowSwitches {
            get => _allow_switches;
            set {
                _allow_switches = value;
                if (!value) {
                    _set_switches = 0;
                    UpdateColors();
                }
            }
        }
        private bool _allow_switches = false;

        public double LabelFontSize {
            get => _font_size;
            set {
                _font_size = value;
                UpdateLabels();
            }
        }
        private double _font_size = 18;

        public Color ActiveColor {
            get => _active_color;
            set {
                _active_color = value;
                UpdateColors();
            }
        }
        private Color _active_color = Colors.PaleGreen;

        public BitField() {
            InitializeComponent();

            _label_objects = [bit0, bit1, bit2, bit3, bit4, bit5, bit6, bit7];
        }

        private void UpdateLabels() {
            for (int i = 0; i < _label_objects.Length; i++) {
                _label_objects[i].Content = _labels[i];
                _label_objects[i].FontSize = _font_size;
            }
        }

        private void UpdateColors() {
            var offBrush = new SolidColorBrush(offColor);
            var onBrush = new SolidColorBrush(_active_color);
            var switchedOffBrush = new RadialGradientBrush(offColor, _active_color);
            var switchedOnBrush = new RadialGradientBrush(_active_color, offColor);

            for (int i = 0; i < _label_objects.Length; i++) {
                if ((_value & (1 << i)) > 0) {
                    if ((_set_switches & (1 << i)) > 0) {
                        _label_objects[i].Background = switchedOffBrush;
                    } else {
                        _label_objects[i].Background = onBrush;
                    }
                } else {
                    if ((_set_switches & (1 << i)) > 0) {
                        _label_objects[i].Background = switchedOnBrush;
                    } else {
                        _label_objects[i].Background = offBrush;
                    }
                }
            }
        }

        private void OnBitClick(object sender, MouseButtonEventArgs e) {
            if (!_allow_switches) return;

            int i = int.Parse((string)((Label)sender).Tag);
            _set_switches ^= (byte)(1 << i);
            UpdateColors();
        }

        public void ResetSwitches() {
            _set_switches = 0;
            UpdateColors();
        }
    }
}
