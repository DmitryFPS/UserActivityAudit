using System.Security.Cryptography;
using System.Text;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.Logging;

namespace UserAudit.Shared.Services;

public sealed class EscrowVaultService
{
    private readonly string _vaultPath;
    private readonly string _privateKeyPath;
    private readonly string _publicKeyPath;
    private readonly ILogger<EscrowVaultService> _logger;

    public EscrowVaultService(IConfiguration configuration, ILogger<EscrowVaultService> logger)
    {
        _logger = logger;
        _vaultPath = configuration["UserAudit:Vault:Path"] ?? "./data/vault";
        Directory.CreateDirectory(_vaultPath);
        _privateKeyPath = Path.Combine(_vaultPath, "escrow_rsa_private.pem");
        _publicKeyPath = Path.Combine(_vaultPath, "escrow_rsa_public.pem");
        EnsureKeys();
    }

    public string GetPublicKeyPem() => File.ReadAllText(_publicKeyPath, Encoding.UTF8);

    public string UnwrapDekBase64(string wrappedDekBase64)
    {
        var wrapped = Convert.FromBase64String(wrappedDekBase64);
        using var rsa = RSA.Create();
        rsa.ImportFromPem(File.ReadAllText(_privateKeyPath, Encoding.UTF8));
        var plain = rsa.Decrypt(wrapped, RSAEncryptionPadding.OaepSHA256);
        return Convert.ToBase64String(plain);
    }

    private void EnsureKeys()
    {
        if (File.Exists(_privateKeyPath) && File.Exists(_publicKeyPath))
        {
            return;
        }

        _logger.LogInformation("Generating escrow RSA keypair at {Path}", _vaultPath);
        using var rsa = RSA.Create(4096);
        File.WriteAllText(_privateKeyPath, rsa.ExportRSAPrivateKeyPem(), Encoding.UTF8);
        File.WriteAllText(_publicKeyPath, rsa.ExportRSAPublicKeyPem(), Encoding.UTF8);
    }
}
