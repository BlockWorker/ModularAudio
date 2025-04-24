using Crc;
using System.Configuration;
using System.Data;
using System.Windows;

namespace I2CMonitorApp {

    internal class I2CCrc8 : Crc8Base {
        internal I2CCrc8() : base(0x7f, 0x00, 0x00, false, false, 0x1f) { }
    }

    /// <summary>
    /// Interaction logic for App.xaml
    /// </summary>
    public partial class App : Application {

        internal static I2CDriverLib.I2CDriver i2cd = new();
        internal static string i2c_portname = "";
        private static readonly HashSet<object> i2c_users = [];

        private static readonly I2CCrc8 crc = new();

        internal static bool I2CConnect(object user, string portname) {
            if (i2cd.connected == 0) {
                try {
                    I2CDriverLib.i2c_connect(ref i2cd, portname);
                    i2c_portname = portname;
                } catch {
                    return false;
                }

                if (i2cd.connected == 0) return false;

                I2CDriverLib.i2c_pullups(ref i2cd, 0xff);
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

        internal static bool I2CCrcWrite(byte dev, byte reg, byte[] data) {
            if (i2cd.connected == 0) return false;

            crc.Reset();
            byte crcValue = crc.ComputeHash([(byte)(dev << 1), reg, .. data])[0];

            return I2CDriverLib.I2C_WriteReg(ref i2cd, dev, reg, [.. data, crcValue]);
        }

        internal static bool I2CCrcMultiWrite(byte dev, byte firstReg, int[] regSizes, byte[] data) {
            if (regSizes.Sum() != data.Length) throw new ArgumentException("Register sizes don't add up to given data size!");

            if (i2cd.connected == 0) return false;

            var regCount = regSizes.Length;

            byte[] sendBytes = new byte[data.Length + regCount];

            //first register: include device and register address
            int regSize = regSizes[0];
            crc.Reset();
            byte crcValue = crc.ComputeHash([(byte)(dev << 1), firstReg, .. data.Take(regSize)])[0];
            Array.Copy(data, sendBytes, regSize);
            sendBytes[regSize] = crcValue;

            //subsequent registers: data only
            int dataOffset = regSize;
            int sendBytesOffset = regSize + 1;
            for (int i = 1; i < regCount; i++) {
                regSize = regSizes[i];
                crc.Reset();
                crcValue = crc.ComputeHash(data, dataOffset, regSize)[0];
                Array.Copy(data, dataOffset, sendBytes, sendBytesOffset, regSize);
                dataOffset += regSize;
                sendBytesOffset += regSize;
                sendBytes[sendBytesOffset++] = crcValue;
            }

            return I2CDriverLib.I2C_WriteReg(ref i2cd, dev, firstReg, sendBytes);
        }

        internal static bool I2CCrcRead(byte dev, byte reg, int length, ref byte[] data) {
            if (data.Length < length) throw new ArgumentException("Given data array too small for the received data!");

            if (i2cd.connected == 0) return false;

            byte[] rxBuffer = new byte[length + 1];
            if (!I2CDriverLib.I2C_ReadReg(ref i2cd, dev, reg, length + 1, ref rxBuffer)) return false;

            crc.Reset();
            byte crcValue = crc.ComputeHash([(byte)(dev << 1), reg, (byte)((dev << 1) | 0x01), .. rxBuffer])[0];
            if (crcValue != 0) return false;

            Array.Copy(rxBuffer, data, length);
            return true;
        }

        internal static bool I2CCrcMultiRead(byte dev, byte firstReg, int[] regSizes, ref byte[] data) {
            var totalLength = regSizes.Sum();
            if (data.Length < totalLength) throw new ArgumentException("Given data array too small for the received data!");

            if (i2cd.connected == 0) return false;

            var regCount = regSizes.Length;

            byte[] rxBuffer = new byte[totalLength + regCount];
            if (!I2CDriverLib.I2C_ReadReg(ref i2cd, dev, firstReg, totalLength + regCount, ref rxBuffer)) return false;

            //first register: include device and register address
            int regSize = regSizes[0];
            crc.Reset();
            byte crcValue = crc.ComputeHash([(byte)(dev << 1), firstReg, (byte)((dev << 1) | 0x01), .. rxBuffer.Take(regSize + 1)])[0];
            if (crcValue != 0) return false;
            Array.Copy(rxBuffer, data, regSize);

            //subsequent registers: data only
            int rxBufferOffset = regSize + 1;
            int dataOffset = regSize;
            for (int i = 1; i < regCount; i++) {
                regSize = regSizes[i];
                crc.Reset();
                crcValue = crc.ComputeHash(rxBuffer, rxBufferOffset, regSize + 1)[0];
                if (crcValue != 0) return false;
                Array.Copy(rxBuffer, rxBufferOffset, data, dataOffset, regSize);
                rxBufferOffset += regSize + 1;
                dataOffset += regSize;
            }

            return true;
        }

    }

}
