using Microsoft.Extensions.Logging;
using Microsoft.EntityFrameworkCore;
using UserAudit.Shared.Data;
using UserAudit.Shared.Entities;

namespace UserAudit.Shared.Services;

public sealed class AlertEngine(UserAuditDbContext db, ILogger<AlertEngine> logger)
{
    public async Task<int> ProcessPendingEventsAsync(CancellationToken cancellationToken = default)
    {
        var pending = await db.AuditEvents
            .Include(x => x.Host)
            .Where(x => !x.AlertProcessed)
            .OrderBy(x => x.ReceivedAtUtc)
            .Take(200)
            .ToListAsync(cancellationToken);

        var created = 0;
        foreach (var auditEvent in pending)
        {
            auditEvent.AlertProcessed = true;
            if (!ShouldAlert(auditEvent))
            {
                continue;
            }

            var alert = new AlertRecord
            {
                Id = Guid.NewGuid(),
                HostId = auditEvent.HostId,
                RuleId = BuildRuleId(auditEvent),
                Title = BuildTitle(auditEvent),
                Severity = auditEvent.Severity,
                Detail = auditEvent.PayloadJson,
                SourceEventId = auditEvent.Id,
                CreatedAtUtc = DateTimeOffset.UtcNow,
            };
            db.Alerts.Add(alert);
            created++;
            logger.LogWarning("Alert {RuleId} for host {Host} event {EventId}",
                alert.RuleId, auditEvent.Host?.Hostname, auditEvent.EventId);
        }

        if (pending.Count > 0)
        {
            await db.SaveChangesAsync(cancellationToken);
        }

        return created;
    }

    private static bool ShouldAlert(AuditEventRecord auditEvent)
    {
        if (string.Equals(auditEvent.Category, "tamper", StringComparison.OrdinalIgnoreCase))
        {
            return true;
        }

        return string.Equals(auditEvent.Severity, "critical", StringComparison.OrdinalIgnoreCase);
    }

    private static string BuildRuleId(AuditEventRecord auditEvent) =>
        $"{auditEvent.Category}.{auditEvent.Action}".ToLowerInvariant();

    private static string BuildTitle(AuditEventRecord auditEvent) =>
        $"[{auditEvent.Host?.Hostname}] {auditEvent.Category}.{auditEvent.Action}";
}
