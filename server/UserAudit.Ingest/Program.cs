using System.Globalization;
using Microsoft.EntityFrameworkCore;
using UserAudit.Shared;
using UserAudit.Shared.Data;
using UserAudit.Shared.Dtos;
using UserAudit.Shared.Entities;
using UserAudit.Shared.Services;

var builder = WebApplication.CreateBuilder(args);

var connectionString = builder.Configuration.GetConnectionString("Default")
    ?? builder.Configuration["UserAudit:Database:ConnectionString"]
    ?? "Host=localhost;Port=5432;Database=useraudit;Username=useraudit;Password=useraudit";

builder.Services.AddUserAuditDatabase(connectionString);
builder.Services.AddScoped<HostRegistry>();
builder.Services.AddScoped<AlertEngine>();

var app = builder.Build();

await app.Services.EnsureUserAuditDatabaseAsync();

app.MapGet("/health", () => Results.Ok(new { status = "ok", service = "ingest" }));

app.MapPost("/api/v1/ingest/logs", async (
    HttpRequest request,
    UserAuditDbContext db,
    HostRegistry hosts,
    IConfiguration config,
    CancellationToken cancellationToken) =>
{
    if (!request.Headers.TryGetValue("X-Log-File", out var fileNameValues) ||
        string.IsNullOrWhiteSpace(fileNameValues.ToString()))
    {
        return Results.BadRequest(new { error = "X-Log-File header required" });
    }

    var fileName = Path.GetFileName(fileNameValues.ToString());
    var hostname = request.Headers.TryGetValue("X-Host-Id", out var hostValues) &&
                   !string.IsNullOrWhiteSpace(hostValues.ToString())
        ? hostValues.ToString()!
        : fileName.Split('-').FirstOrDefault() ?? "UNKNOWN";

    await using var ms = new MemoryStream();
    await request.Body.CopyToAsync(ms, cancellationToken);
    var body = ms.ToArray();
    if (body.Length == 0)
    {
        return Results.BadRequest(new { error = "empty body" });
    }

    var host = await hosts.TouchAsync(hostname, cancellationToken);
    var storageRoot = config["UserAudit:Storage:Path"] ?? "./data/logs";
    Directory.CreateDirectory(Path.Combine(storageRoot, host.Hostname));

    var storedName = $"{DateTimeOffset.UtcNow:yyyyMMddHHmmssfff}_{fileName}";
    var storedPath = Path.Combine(storageRoot, host.Hostname, storedName);
    await File.WriteAllBytesAsync(storedPath, body, cancellationToken);

    var blob = new LogBlobRecord
    {
        Id = Guid.NewGuid(),
        HostId = host.Id,
        FileName = fileName,
        SizeBytes = body.Length,
        Sha256Hex = Hashing.Sha256Hex(body),
        StoredPath = storedPath,
        UploadedAtUtc = DateTimeOffset.UtcNow,
    };
    db.LogBlobs.Add(blob);
    await db.SaveChangesAsync(cancellationToken);

    return Results.Created($"/api/v1/ingest/logs/{blob.Id}", new
    {
        blob.Id,
        host = host.Hostname,
        blob.FileName,
        blob.SizeBytes,
        blob.UploadedAtUtc,
    });
});

app.MapPost("/api/v1/ingest/events", async (
    IngestEventDto dto,
    UserAuditDbContext db,
    HostRegistry hosts,
    AlertEngine alerts,
    CancellationToken cancellationToken) =>
{
    if (string.IsNullOrWhiteSpace(dto.EventId) || string.IsNullOrWhiteSpace(dto.Host))
    {
        return Results.BadRequest(new { error = "EventId and Host required" });
    }

    if (await db.AuditEvents.AnyAsync(x => x.EventId == dto.EventId, cancellationToken))
    {
        return Results.Ok(new { status = "duplicate" });
    }

    var host = await hosts.TouchAsync(dto.Host, cancellationToken);
    if (!DateTimeOffset.TryParse(dto.Timestamp, CultureInfo.InvariantCulture,
            DateTimeStyles.AssumeUniversal | DateTimeStyles.AdjustToUniversal, out var ts))
    {
        ts = DateTimeOffset.UtcNow;
    }

    var record = new AuditEventRecord
    {
        Id = Guid.NewGuid(),
        HostId = host.Id,
        EventId = dto.EventId,
        TimestampUtc = ts,
        Category = dto.Category,
        Action = dto.Action,
        Severity = dto.Severity,
        PayloadJson = dto.PayloadJson,
        ReceivedAtUtc = DateTimeOffset.UtcNow,
        AlertProcessed = false,
    };
    db.AuditEvents.Add(record);
    await db.SaveChangesAsync(cancellationToken);
    await alerts.ProcessPendingEventsAsync(cancellationToken);

    return Results.Accepted($"/api/v1/ingest/events/{record.EventId}", new { record.EventId });
});

app.MapGet("/api/v1/ingest/hosts", async (UserAuditDbContext db, CancellationToken cancellationToken) =>
{
    var hosts = await db.Hosts
        .Select(h => new HostSummaryDto(
            h.Hostname,
            h.FirstSeenUtc,
            h.LastSeenUtc,
            h.LogBlobs.Count,
            h.Alerts.Count(a => a.AcknowledgedAtUtc == null)))
        .OrderBy(h => h.Hostname)
        .ToListAsync(cancellationToken);
    return Results.Ok(hosts);
});

app.Run();
