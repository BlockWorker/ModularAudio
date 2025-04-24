using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection.Metadata;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace I2CMonitorApp {
    public partial class I2CDriverLib {

        [StructLayout(LayoutKind.Sequential)]
        public struct I2CDriver {
            public int connected;          // Set to 1 when connected
            IntPtr port;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 16)] public string model = new(' ', 16);
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 9)] public string serial = new(' ', 9);    // Serial number of USB device
            public ulong uptime;       // time since boot (seconds)
            public float voltage_v,    // USB voltage (Volts)
                current_ma,   // device current (mA)
                temp_celsius; // temperature (C)
            public uint mode;      // I2C 'I' or bitbang 'B' mode
            public uint sda;       // SDA state, 0 or 1
            public uint scl;       // SCL state, 0 or 1
            public uint speed;     // I2C line speed (in kHz)
            public uint pullups;   // pullup state (6 bits, 1=enabled)
            public uint
                ccitt_crc,    // Hardware CCITT CRC
                e_ccitt_crc;  // Host CCITT CRC, should match

            public I2CDriver() {
            }
        }


        [DllImport("I2CDriverLib.dll")]
        internal static extern void i2c_connect(ref I2CDriver sd, string portname);

        [DllImport("I2CDriverLib.dll")]
        internal static extern void i2c_disconnect(ref I2CDriver sd);

        [DllImport("I2CDriverLib.dll")]
        internal static extern void i2c_getstatus(ref I2CDriver sd);

        [DllImport("I2CDriverLib.dll")]
        internal static extern int i2c_write(ref I2CDriver sd, byte[] bytes, nuint nn);

        [DllImport("I2CDriverLib.dll")]
        internal static extern void i2c_read(ref I2CDriver sd, [Out] byte[] bytes, nuint nn);

        [DllImport("I2CDriverLib.dll")]
        internal static extern int i2c_start(ref I2CDriver sd, byte dev, byte op);

        [DllImport("I2CDriverLib.dll")]
        internal static extern void i2c_stop(ref I2CDriver sd);

        [DllImport("I2CDriverLib.dll")]
        internal static extern void i2c_regrd(ref I2CDriver sd, byte dev, byte reg, [Out] byte[] bytes, byte nn);

        [DllImport("I2CDriverLib.dll")]
        internal static extern void i2c_monitor(ref I2CDriver sd, int enable);

        [DllImport("I2CDriverLib.dll")]
        internal static extern void i2c_capture(ref I2CDriver sd);

        [DllImport("I2CDriverLib.dll")]
        internal static extern void i2c_pullups(ref I2CDriver sd, byte pullups);

        [DllImport("I2CDriverLib.dll")]
        internal static extern int i2c_commands(ref I2CDriver sd, int argc, string[] argv);


        internal static bool I2C_WriteReg(ref I2CDriver sd, byte dev, byte reg, byte[] data) {
            if (sd.connected == 0) return false;

            byte[] fulldata = new byte[data.Length + 1];
            fulldata[0] = reg;
            Array.Copy(data, 0, fulldata, 1, data.Length);

            int r = i2c_start(ref sd, dev, 0);
            if (r == 0) {
                i2c_stop(ref sd);
                i2c_getstatus(ref sd);
                return false;
            }

            r = i2c_write(ref sd, fulldata, (nuint)fulldata.LongLength);
            i2c_stop(ref sd);
            i2c_getstatus(ref sd);
            return r != 0;
        }

        internal static bool I2C_ReadReg(ref I2CDriver sd, byte dev, byte reg, int length, ref byte[] data) {
            if (sd.connected == 0 || length > 0xFF) return false;

            i2c_regrd(ref sd, dev, reg, data, (byte)(length & 0xFF));

            return true;

            /*byte[] wdata = [reg];

            int r = i2c_start(ref sd, dev, 0);
            if (r == 0) {
                i2c_stop(ref sd);
                i2c_getstatus(ref sd);
                return false;
            }

            r = i2c_write(ref sd, wdata, 1);
            if (r == 0) {
                i2c_stop(ref sd);
                i2c_getstatus(ref sd);
                return false;
            }

            r = i2c_start(ref sd, dev, 1);
            if (r == 0) {
                i2c_stop(ref sd);
                i2c_getstatus(ref sd);
                return false;
            }

            i2c_read(ref sd, data, (nuint)length);
            i2c_stop(ref sd);
            i2c_getstatus(ref sd);
            return true;*/
        }

    }
}
