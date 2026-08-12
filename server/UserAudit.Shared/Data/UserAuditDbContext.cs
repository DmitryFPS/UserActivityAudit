using Microsoft.EntityFrameworkCore;
using UserAudit.Shared.Entities;

namespace UserAudit.Shared.Data;

public sealed class UserAuditDbContext(DbContextOptions<UserAuditDbContext> options) : DbContext(options)
{
    public DbSet<HostRecord> Hosts => Set<HostRecord>();
    public DbSet<LogBlobRecord> LogBlobs => Set<LogBlobRecord>();
    public DbSet<EscrowRecord> EscrowRecords => Set<EscrowRecord>();
    public DbSet<AuditEventRecord> AuditEvents => Set<AuditEventRecord>();
    public DbSet<AlertRecord> Alerts => Set<AlertRecord>();

    protected override void OnModelCreating(ModelBuilder modelBuilder)
    {
        modelBuilder.Entity<HostRecord>(entity =>
        {
            entity.HasIndex(x => x.Hostname).IsUnique();
            entity.Property(x => x.Hostname).HasMaxLength(128);
        });

        modelBuilder.Entity<LogBlobRecord>(entity =>
        {
            entity.HasIndex(x => new { x.HostId, x.FileName, x.UploadedAtUtc });
            entity.Property(x => x.FileName).HasMaxLength(260);
            entity.Property(x => x.Sha256Hex).HasMaxLength(64);
            entity.Property(x => x.StoredPath).HasMaxLength(512);
        });

        modelBuilder.Entity<EscrowRecord>(entity =>
        {
            entity.HasIndex(x => new { x.HostId, x.IsActive });
        });

        modelBuilder.Entity<AuditEventRecord>(entity =>
        {
            entity.HasIndex(x => x.EventId).IsUnique();
            entity.HasIndex(x => new { x.HostId, x.TimestampUtc });
            entity.Property(x => x.EventId).HasMaxLength(64);
            entity.Property(x => x.Category).HasMaxLength(32);
            entity.Property(x => x.Action).HasMaxLength(64);
            entity.Property(x => x.Severity).HasMaxLength(16);
        });

        modelBuilder.Entity<AlertRecord>(entity =>
        {
            entity.HasIndex(x => new { x.HostId, x.CreatedAtUtc });
            entity.Property(x => x.RuleId).HasMaxLength(64);
            entity.Property(x => x.Title).HasMaxLength(256);
            entity.Property(x => x.Severity).HasMaxLength(16);
        });
    }
}
