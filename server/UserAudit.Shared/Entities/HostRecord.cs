namespace UserAudit.Shared.Entities;

public sealed class HostRecord
{
    public Guid Id { get; set; }
    public required string Hostname { get; set; }
    public DateTimeOffset FirstSeenUtc { get; set; }
    public DateTimeOffset LastSeenUtc { get; set; }
    public string? AgentVersion { get; set; }

    public ICollection<LogBlobRecord> LogBlobs { get; set; } = [];
    public ICollection<AuditEventRecord> Events { get; set; } = [];
    public ICollection<EscrowRecord> EscrowRecords { get; set; } = [];
    public ICollection<AlertRecord> Alerts { get; set; } = [];
}
