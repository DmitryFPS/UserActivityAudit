using System.Security.Cryptography;

namespace UserAudit.Admin.Core.Crypto;

public static class EncryptedLogReader
{
    public const string LinePrefix = "v1:";
    private const int KeySize = 32;
    private const int NonceSize = 12;
    private const int TagSize = 16;

    public static bool TryDecryptLine(ReadOnlySpan<byte> dek, string encryptedLine, out string plaintext)
    {
        plaintext = string.Empty;
        if (dek.Length != KeySize || !encryptedLine.StartsWith(LinePrefix, StringComparison.Ordinal))
        {
            return false;
        }

        var encoded = encryptedLine[LinePrefix.Length..];
        byte[] payload;
        try
        {
            payload = Convert.FromBase64String(encoded);
        }
        catch (FormatException)
        {
            return false;
        }

        if (payload.Length < NonceSize + TagSize)
        {
            return false;
        }

        var nonce = payload.AsSpan(0, NonceSize);
        var tag = payload.AsSpan(payload.Length - TagSize, TagSize);
        var ciphertext = payload.AsSpan(NonceSize, payload.Length - NonceSize - TagSize);

        var plainBytes = new byte[ciphertext.Length];
        try
        {
            using var aes = new AesGcm(dek, TagSize);
            aes.Decrypt(nonce, ciphertext, tag, plainBytes);
        }
        catch (CryptographicException)
        {
            return false;
        }

        plaintext = System.Text.Encoding.UTF8.GetString(plainBytes);
        return true;
    }

    public static byte[] LoadDekFromFile(string path)
    {
        var bytes = File.ReadAllBytes(path);
        if (bytes.Length == KeySize)
        {
            return bytes;
        }

        var text = System.Text.Encoding.UTF8.GetString(bytes).Trim();
        if (TryDecodeBase64Key(text, out var decoded))
        {
            return decoded;
        }

        throw new InvalidOperationException($"DEK file must be {KeySize} raw bytes or base64-encoded key.");
    }

    public static byte[] DecodeDekBase64(string base64)
    {
        if (!TryDecodeBase64Key(base64.Trim(), out var dek))
        {
            throw new InvalidOperationException("Invalid DEK base64.");
        }

        return dek;
    }

    private static bool TryDecodeBase64Key(string text, out byte[] dek)
    {
        dek = [];
        try
        {
            var bytes = Convert.FromBase64String(text);
            if (bytes.Length != KeySize)
            {
                return false;
            }

            dek = bytes;
            return true;
        }
        catch (FormatException)
        {
            return false;
        }
    }
}
