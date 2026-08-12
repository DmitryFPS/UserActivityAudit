namespace UserAudit.Shared.Entities;

public sealed class EscrowRecord
{
    public Guid Id { get; set; }
    public Guid HostId { get; set; }
    public required string WrappedDekBase64 { get; set; }
    public DateTimeOffset CreatedAtUtc { get; set; }
    public DateTimeOffset? RotatedAtUtc { get; set; }
    public bool IsActive { get; set; } = true;

    public HostRecord? Host { get; set; }
}
