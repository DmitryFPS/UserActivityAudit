using UserAudit.Admin.Core.Crypto;
using UserAudit.Admin.Core.Models;

namespace UserAudit.Admin.Core.Import;

public sealed class LocalLogService(LogImporter importer)
{
    public string LogsDirectory { get; set; } = LocalKeyProvider.DefaultLogsDirectory;
    public string KeysDirectory { get; set; } = LocalKeyProvider.DefaultKeysDirectory;
    public string? ExternalDekFilePath { get; set; }

    public LocalImportResult ImportAll()
    {
        if (!Directory.Exists(LogsDirectory))
        {
            return LocalImportResult.Fail($"Каталог логов не найден: {LogsDirectory}");
        }

        var dekResult = ResolveDek();
        if (!dekResult.Success)
        {
            return LocalImportResult.Fail(dekResult.Error!);
        }

        var events = importer.ImportDirectory(LogsDirectory, dekResult.Dek!);
        return LocalImportResult.Ok(events, dekResult.Source);
    }

    private DekResolveResult ResolveDek()
    {
        if (!string.IsNullOrWhiteSpace(ExternalDekFilePath))
        {
            try
            {
                return DekResolveResult.Ok(EncryptedLogReader.LoadDekFromFile(ExternalDekFilePath), "файл DEK");
            }
            catch (Exception ex)
            {
                return DekResolveResult.Fail(ex.Message);
            }
        }

        if (LocalKeyProvider.TryLoadDek(KeysDirectory, out var dek, out var error))
        {
            return DekResolveResult.Ok(dek, "DPAPI (этот ПК)");
        }

        return DekResolveResult.Fail(error ?? "Ключ недоступен.");
    }

    private sealed record DekResolveResult(bool Success, byte[]? Dek, string? Source, string? Error)
    {
        public static DekResolveResult Ok(byte[] dek, string source) => new(true, dek, source, null);
        public static DekResolveResult Fail(string error) => new(false, null, null, error);
    }
}

public sealed record LocalImportResult(
    bool Success,
    IReadOnlyList<AuditEventModel> Events,
    string? KeySource,
    string? Error)
{
    public static LocalImportResult Ok(IReadOnlyList<AuditEventModel> events, string? keySource) =>
        new(true, events, keySource, null);

    public static LocalImportResult Fail(string error) =>
        new(false, [], null, error);
}
