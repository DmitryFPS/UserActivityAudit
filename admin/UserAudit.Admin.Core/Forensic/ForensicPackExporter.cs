using System.IO.Compression;
using System.Text.Json;
using UserAudit.Admin.Core.Models;

namespace UserAudit.Admin.Core.Forensic;

public static class ForensicPackExporter
{
    public static void ExportZip(IReadOnlyList<AuditEventModel> events, string outputZipPath, string? hostname = null)
    {
        var tempDir = Path.Combine(Path.GetTempPath(), "useraudit-pack-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(tempDir);

        try
        {
            var eventsPath = Path.Combine(tempDir, "events.jsonl");
            using (var writer = new StreamWriter(eventsPath, false, System.Text.Encoding.UTF8))
            {
                foreach (var evt in events.OrderBy(x => x.TimestampUtc))
                {
                    var line = JsonSerializer.Serialize(new
                    {
                        id = evt.EventId,
                        ts = evt.TimestampUtc.ToString("O"),
                        cat = evt.Category,
                        act = evt.Action,
                        sev = evt.Severity,
                        host = evt.Host,
                        corr = evt.CorrelationId,
                        data = evt.Data,
                    });
                    writer.WriteLine(line);
                }
            }

            var manifestPath = Path.Combine(tempDir, "manifest.json");
            var manifest = new
            {
                exportedAtUtc = DateTimeOffset.UtcNow,
                hostname,
                eventCount = events.Count,
                categories = events.GroupBy(x => x.Category).ToDictionary(g => g.Key, g => g.Count()),
            };
            File.WriteAllText(manifestPath, JsonSerializer.Serialize(manifest, new JsonSerializerOptions { WriteIndented = true }));

            if (File.Exists(outputZipPath))
            {
                File.Delete(outputZipPath);
            }

            ZipFile.CreateFromDirectory(tempDir, outputZipPath);
        }
        finally
        {
            if (Directory.Exists(tempDir))
            {
                Directory.Delete(tempDir, true);
            }
        }
    }
}
