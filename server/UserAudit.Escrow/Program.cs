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
builder.Services.AddSingleton<EscrowVaultService>();
builder.Services.AddScoped<HostRegistry>();

var app = builder.Build();
await app.Services.EnsureUserAuditDatabaseAsync();

app.MapGet("/health", () => Results.Ok(new { status = "ok", service = "escrow" }));

app.MapGet("/api/v1/escrow/public-key", (EscrowVaultService vault) =>
    Results.Text(vault.GetPublicKeyPem(), "application/x-pem-file"));

app.MapPost("/api/v1/escrow/wrap", async (
    WrapDekRequest request,
    UserAuditDbContext db,
    HostRegistry hosts,
    CancellationToken cancellationToken) =>
{
    if (string.IsNullOrWhiteSpace(request.Hostname) || string.IsNullOrWhiteSpace(request.WrappedDekBase64))
    {
        return Results.BadRequest(new { error = "Hostname and WrappedDekBase64 required" });
    }

    var host = await hosts.TouchAsync(request.Hostname, cancellationToken);
    var active = await db.EscrowRecords.Where(x => x.HostId == host.Id && x.IsActive).ToListAsync(cancellationToken);
    foreach (var item in active)
    {
        item.IsActive = false;
        item.RotatedAtUtc = DateTimeOffset.UtcNow;
    }

    var record = new EscrowRecord
    {
        Id = Guid.NewGuid(),
        HostId = host.Id,
        WrappedDekBase64 = request.WrappedDekBase64,
        CreatedAtUtc = DateTimeOffset.UtcNow,
        IsActive = true,
    };
    db.EscrowRecords.Add(record);
    await db.SaveChangesAsync(cancellationToken);
    return Results.Created($"/api/v1/escrow/records/{record.Id}", new { record.Id, host = host.Hostname });
});

app.MapPost("/api/v1/escrow/unwrap", (
    WrapDekRequest request,
    HttpContext context,
    IConfiguration configuration,
    EscrowVaultService vault,
    UserAuditDbContext db) =>
{
    if (!AdminAuth.IsAuthorized(context.Request.Headers, configuration))
    {
        return Results.Unauthorized();
    }

    if (string.IsNullOrWhiteSpace(request.Hostname))
    {
        return Results.BadRequest(new { error = "Hostname required" });
    }

    var host = db.Hosts.FirstOrDefault(x => x.Hostname == request.Hostname.Trim().ToUpperInvariant());
    if (host is null)
    {
        return Results.NotFound(new { error = "host not found" });
    }

    var record = db.EscrowRecords
        .Where(x => x.HostId == host.Id && x.IsActive)
        .OrderByDescending(x => x.CreatedAtUtc)
        .FirstOrDefault();

    if (record is null)
    {
        return Results.NotFound(new { error = "no active escrow record" });
    }

    var wrapped = string.IsNullOrWhiteSpace(request.WrappedDekBase64)
        ? record.WrappedDekBase64
        : request.WrappedDekBase64;

    try
    {
        var dek = vault.UnwrapDekBase64(wrapped);
        return Results.Ok(new UnwrapDekResponse(host.Hostname, dek));
    }
    catch (Exception ex)
    {
        return Results.BadRequest(new { error = ex.Message });
    }
});

app.Run();
