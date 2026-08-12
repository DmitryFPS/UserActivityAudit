namespace UserAudit.Shared.Entities;

public sealed class LogBlobRecord
{
    public Guid Id { get; set; }
    public Guid HostId { get; set; }
    public required string FileName { get; set; }
    public long SizeBytes { get; set; }
    public required string Sha256Hex { get; set; }
    public required string StoredPath { get; set; }
    public DateTimeOffset UploadedAtUtc { get; set; }

    public HostRecord? Host { get; set; }
}
