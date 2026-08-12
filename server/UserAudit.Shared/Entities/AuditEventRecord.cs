namespace UserAudit.Shared.Entities;

public sealed class AuditEventRecord
{
    public Guid Id { get; set; }
    public Guid HostId { get; set; }
    public required string EventId { get; set; }
    public DateTimeOffset TimestampUtc { get; set; }
    public required string Category { get; set; }
    public required string Action { get; set; }
    public required string Severity { get; set; }
    public string? PayloadJson { get; set; }
    public DateTimeOffset ReceivedAtUtc { get; set; }
    public bool AlertProcessed { get; set; }

    public HostRecord? Host { get; set; }
}
