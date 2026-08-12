namespace UserAudit.Shared.Entities;

public sealed class AlertRecord
{
    public Guid Id { get; set; }
    public Guid HostId { get; set; }
    public required string RuleId { get; set; }
    public required string Title { get; set; }
    public required string Severity { get; set; }
    public string? Detail { get; set; }
    public Guid? SourceEventId { get; set; }
    public DateTimeOffset CreatedAtUtc { get; set; }
    public DateTimeOffset? AcknowledgedAtUtc { get; set; }

    public HostRecord? Host { get; set; }
}
