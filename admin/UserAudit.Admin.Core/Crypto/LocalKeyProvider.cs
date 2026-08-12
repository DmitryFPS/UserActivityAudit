using System.Runtime.InteropServices;

namespace UserAudit.Admin.Core.Crypto;

/// <summary>
/// Reads DEK from agent key file (DPAPI LocalMachine), same as native KeyManager.
/// Requires elevated/admin on the PC where the agent wrote logs.
/// </summary>
public static class LocalKeyProvider
{
    public const int DekSize = 32;
    private const int HmacKeySize = 32;
    private const int CryptProtectLocalMachine = 0x4;

    public static string DefaultKeysDirectory =>
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), "UserAudit", "keys");

    public static string DefaultLogsDirectory =>
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), "UserAudit", "logs");

    public static string MasterKeyFileName => "master.key.dpapi";

    public static bool TryLoadDek(string keysDirectory, out byte[] dek, out string? error)
    {
        dek = [];
        error = null;
        var path = Path.Combine(keysDirectory, MasterKeyFileName);
        if (!File.Exists(path))
        {
            error = $"Ключ не найден: {path}. Установите агент на этом ПК или укажите файл DEK вручную.";
            return false;
        }

        try
        {
            var encrypted = File.ReadAllBytes(path);
            if (!CryptUnprotectData(encrypted, CryptProtectLocalMachine, out var plain))
            {
                error = "Не удалось расшифровать ключ DPAPI. Запустите анализатор от имени администратора на том же ПК, где работает агент.";
                return false;
            }

            if (plain.Length != DekSize + HmacKeySize)
            {
                error = "Неверный размер ключа после DPAPI.";
                return false;
            }

            dek = plain.AsSpan(0, DekSize).ToArray();
            return true;
        }
        catch (Exception ex)
        {
            error = ex.Message;
            return false;
        }
    }

    private static bool CryptUnprotectData(byte[] encrypted, int flags, out byte[] plain)
    {
        plain = [];
        var input = new DataBlob
        {
            cbData = (uint)encrypted.Length,
            pbData = Marshal.AllocHGlobal(encrypted.Length),
        };

        try
        {
            Marshal.Copy(encrypted, 0, input.pbData, encrypted.Length);
            var output = new DataBlob();
            var ok = CryptUnprotectData(ref input, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero, flags,
                ref output);
            if (!ok)
            {
                return false;
            }

            plain = new byte[output.cbData];
            Marshal.Copy(output.pbData, plain, 0, (int)output.cbData);
            LocalFree(output.pbData);
            return true;
        }
        finally
        {
            if (input.pbData != IntPtr.Zero)
            {
                Marshal.FreeHGlobal(input.pbData);
            }
        }
    }

    [DllImport("crypt32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern bool CryptUnprotectData(
        ref DataBlob pDataIn,
        IntPtr ppszDataDescr,
        IntPtr pOptionalEntropy,
        IntPtr pvReserved,
        IntPtr pPromptStruct,
        int dwFlags,
        ref DataBlob pDataOut);

    [DllImport("kernel32.dll")]
    private static extern IntPtr LocalFree(IntPtr hMem);

    [StructLayout(LayoutKind.Sequential)]
    private struct DataBlob
    {
        public uint cbData;
        public IntPtr pbData;
    }
}
