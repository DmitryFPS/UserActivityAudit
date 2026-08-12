using System.Security.Cryptography;
using Microsoft.EntityFrameworkCore;
using UserAudit.Shared.Data;
using UserAudit.Shared.Entities;

namespace UserAudit.Shared.Services;

public sealed class HostRegistry(UserAuditDbContext db)
{
    public async Task<HostRecord> TouchAsync(string hostname, CancellationToken cancellationToken = default)
    {
        var normalized = hostname.Trim().ToUpperInvariant();
        var host = await db.Hosts.FirstOrDefaultAsync(x => x.Hostname == normalized, cancellationToken);
        var now = DateTimeOffset.UtcNow;
        if (host is null)
        {
            host = new HostRecord
            {
                Id = Guid.NewGuid(),
                Hostname = normalized,
                FirstSeenUtc = now,
                LastSeenUtc = now,
            };
            db.Hosts.Add(host);
        }
        else
        {
            host.LastSeenUtc = now;
        }

        await db.SaveChangesAsync(cancellationToken);
        return host;
    }
}

public static class Hashing
{
    public static string Sha256Hex(ReadOnlySpan<byte> data)
    {
        Span<byte> hash = stackalloc byte[32];
        SHA256.HashData(data, hash);
        return Convert.ToHexString(hash).ToLowerInvariant();
    }
}
