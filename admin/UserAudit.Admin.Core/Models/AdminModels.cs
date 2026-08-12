namespace UserAudit.Admin.Core.Models;

public sealed class AuditEventModel
{
    public required string EventId { get; init; }
    public DateTimeOffset TimestampUtc { get; init; }
    public int Level { get; init; }
    public required string Category { get; init; }
    public required string Action { get; init; }
    public required string Severity { get; init; }
    public required string Host { get; init; }
    public string? User { get; init; }
    public string? Source { get; init; }
    public string? CorrelationId { get; init; }
    public IReadOnlyDictionary<string, string> Data { get; init; } = new Dictionary<string, string>();
    public string? SourceFile { get; init; }
}

public sealed record HostSummary(
    string Hostname,
    DateTimeOffset FirstSeenUtc,
    DateTimeOffset LastSeenUtc,
    int LogBlobCount,
    int OpenAlertCount);

public sealed record TimelineEvent(
    string EventId,
    string Hostname,
    DateTimeOffset TimestampUtc,
    string Category,
    string Action,
    string Severity,
    string? PayloadJson);

public sealed record AlertSummary(
    Guid Id,
    string Hostname,
    string RuleId,
    string Title,
    string Severity,
    string? Detail,
    DateTimeOffset CreatedAtUtc,
    DateTimeOffset? AcknowledgedAtUtc);
