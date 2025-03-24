using System.Configuration;
using System.Data;
using System.Windows;

namespace I2CMonitorApp {
    /// <summary>
    /// Interaction logic for App.xaml
    /// </summary>
    public partial class App : Application {

        internal static I2CDriverLib.I2CDriver i2cd = new();
        internal static string i2c_portname = "";
        private static readonly HashSet<object> i2c_users = [];

        internal static bool I2CConnect(object user, string portname) {
            if (i2cd.connected == 0) {
                try {
                    I2CDriverLib.i2c_connect(ref i2cd, portname);
                    i2c_portname = portname;
                } catch {
                    return false;
                }

                if (i2cd.connected == 0) return false;
            }

            i2c_users.Add(user);
            return true;
        }

        internal static void I2CDisconnect(object user) {
            i2c_users.Remove(user);

            if (i2c_users.Count == 0 && i2cd.connected != 0) {
                I2CDriverLib.i2c_disconnect(ref i2cd);
                i2c_portname = "";
            }
        }

    }

}
