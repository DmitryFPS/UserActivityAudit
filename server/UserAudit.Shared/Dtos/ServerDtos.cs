namespace UserAudit.Shared.Dtos;

public sealed record IngestEventDto(
    string EventId,
    string Timestamp,
    string Category,
    string Action,
    string Severity,
    string Host,
    string? PayloadJson);

public sealed record WrapDekRequest(string Hostname, string WrappedDekBase64);

public sealed record UnwrapDekResponse(string Hostname, string WrappedDekBase64);

public sealed record AlertDto(
    Guid Id,
    string Hostname,
    string RuleId,
    string Title,
    string Severity,
    string? Detail,
    DateTimeOffset CreatedAtUtc,
    DateTimeOffset? AcknowledgedAtUtc);

public sealed record HostSummaryDto(
    string Hostname,
    DateTimeOffset FirstSeenUtc,
    DateTimeOffset LastSeenUtc,
    int LogBlobCount,
    int OpenAlertCount);

public sealed record TimelineEventDto(
    string EventId,
    string Hostname,
    DateTimeOffset TimestampUtc,
    string Category,
    string Action,
    string Severity,
    string? PayloadJson);
