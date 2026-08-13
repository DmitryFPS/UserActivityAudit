using UserAudit.Admin.Core.Models;

namespace UserAudit.Admin.Core.Import;

public static class LocalAnalytics
{
    public static IReadOnlyList<LocalHostRow> BuildHosts(IEnumerable<AuditEventModel> events) =>
        events
            .GroupBy(x => x.Host, StringComparer.OrdinalIgnoreCase)
            .Select(g =>
            {
                var ordered = g.OrderBy(x => x.TimestampUtc).ToList();
                var tamper = g.Count(x =>
                    x.Category.Equals("tamper", StringComparison.OrdinalIgnoreCase));
                return new LocalHostRow(
                    g.Key,
                    ordered.First().TimestampUtc,
                    ordered.Last().TimestampUtc,
                    g.Count(),
                    tamper);
            })
            .OrderBy(x => x.Hostname)
            .ToList();

    public static IReadOnlyList<LocalAlertRow> BuildAlerts(IEnumerable<AuditEventModel> events) =>
        events
            .Where(x =>
                x.Category.Equals("tamper", StringComparison.OrdinalIgnoreCase) ||
                x.Severity.Equals("critical", StringComparison.OrdinalIgnoreCase))
            .OrderByDescending(x => x.TimestampUtc)
            .Select(x => new LocalAlertRow(
                x.TimestampUtc,
                x.Host,
                $"{x.Category}.{x.Action}".ToLowerInvariant(),
                BuildAlertTitle(x),
                x.Severity,
                x.Data.TryGetValue("detail", out var d) ? d :
                    x.Data.TryGetValue("reason", out var r) ? r : null))
            .ToList();

    private static string BuildAlertTitle(AuditEventModel x)
    {
        if (x.Data.TryGetValue("reason", out var reason) && !string.IsNullOrWhiteSpace(reason))
        {
            return $"[{x.Host}] {reason}";
        }

        if (x.Data.TryGetValue("detail", out var detail) && !string.IsNullOrWhiteSpace(detail))
        {
            return $"[{x.Host}] {detail}";
        }

        return $"[{x.Host}] {x.Category}.{x.Action}";
    }
}

public sealed record LocalHostRow(
    string Hostname,
    DateTimeOffset FirstSeenUtc,
    DateTimeOffset LastSeenUtc,
    int EventCount,
    int TamperCount);

public sealed record LocalAlertRow(
    DateTimeOffset CreatedAtUtc,
    string Hostname,
    string RuleId,
    string Title,
    string Severity,
    string? Detail);
