using System.Text.Json;
using UserAudit.Admin.Core.Crypto;
using UserAudit.Admin.Core.Models;

namespace UserAudit.Admin.Core.Import;

public sealed class LogImporter
{
    public const string EncryptedExtension = ".jsonl.enc";

    public IReadOnlyList<AuditEventModel> ImportDirectory(string directory, ReadOnlySpan<byte> dek)
    {
        if (!Directory.Exists(directory))
        {
            throw new DirectoryNotFoundException(directory);
        }

        var events = new List<AuditEventModel>();
        foreach (var file in Directory.EnumerateFiles(directory, $"*{EncryptedExtension}", SearchOption.AllDirectories)
                     .OrderBy(x => x, StringComparer.OrdinalIgnoreCase))
        {
            events.AddRange(ImportFile(file, dek));
        }

        return events;
    }

    public IReadOnlyList<AuditEventModel> ImportFile(string filePath, ReadOnlySpan<byte> dek)
    {
        var events = new List<AuditEventModel>();
        using var stream = new FileStream(filePath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
        using var reader = new StreamReader(stream);
        while (reader.ReadLine() is { } line)
        {
            if (string.IsNullOrWhiteSpace(line))
            {
                continue;
            }

            if (!EncryptedLogReader.TryDecryptLine(dek, line.Trim(), out var plaintext))
            {
                continue;
            }

            if (TryParseEvent(plaintext, filePath, out var model))
            {
                events.Add(model);
            }
        }

        return events;
    }

    public static bool TryParseEvent(string json, string? sourceFile, out AuditEventModel model)
    {
        model = null!;
        try
        {
            using var doc = JsonDocument.Parse(json);
            var root = doc.RootElement;
            var id = root.GetProperty("id").GetString() ?? Guid.NewGuid().ToString("N");
            var tsText = root.GetProperty("ts").GetString() ?? DateTimeOffset.UtcNow.ToString("O");
            if (!DateTimeOffset.TryParse(tsText, out var ts))
            {
                ts = DateTimeOffset.UtcNow;
            }

            var data = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            if (root.TryGetProperty("data", out var dataEl) && dataEl.ValueKind == JsonValueKind.Object)
            {
                foreach (var prop in dataEl.EnumerateObject())
                {
                    data[prop.Name] = prop.Value.ValueKind switch
                    {
                        JsonValueKind.String => prop.Value.GetString() ?? string.Empty,
                        JsonValueKind.Number => prop.Value.GetRawText(),
                        JsonValueKind.True => "true",
                        JsonValueKind.False => "false",
                        _ => prop.Value.GetRawText(),
                    };
                }
            }

            model = new AuditEventModel
            {
                EventId = id,
                TimestampUtc = ts.ToUniversalTime(),
                Level = root.TryGetProperty("lvl", out var lvl) ? lvl.GetInt32() : 1,
                Category = root.GetProperty("cat").GetString() ?? "unknown",
                Action = root.GetProperty("act").GetString() ?? "unknown",
                Severity = root.TryGetProperty("sev", out var sev) ? sev.GetString() ?? "info" : "info",
                Host = root.TryGetProperty("host", out var host) ? host.GetString() ?? "UNKNOWN" : "UNKNOWN",
                User = root.TryGetProperty("user", out var user) ? user.GetString() : null,
                Source = root.TryGetProperty("src", out var src) ? src.GetString() : null,
                CorrelationId = root.TryGetProperty("corr", out var corr) ? corr.GetString() : null,
                Data = data,
                SourceFile = sourceFile,
            };
            return true;
        }
        catch (JsonException)
        {
            return false;
        }
        catch (KeyNotFoundException)
        {
            return false;
        }
    }
}
