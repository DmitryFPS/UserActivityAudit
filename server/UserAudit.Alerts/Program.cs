using Microsoft.EntityFrameworkCore;
using UserAudit.Shared;
using UserAudit.Shared.Data;
using UserAudit.Shared.Dtos;
using UserAudit.Shared.Services;

var builder = WebApplication.CreateBuilder(args);

var connectionString = builder.Configuration.GetConnectionString("Default")
    ?? builder.Configuration["UserAudit:Database:ConnectionString"]
    ?? "Host=localhost;Port=5432;Database=useraudit;Username=useraudit;Password=useraudit";

builder.Services.AddUserAuditDatabase(connectionString);
builder.Services.AddScoped<AlertEngine>();
builder.Services.AddHostedService<AlertPollingService>();

var app = builder.Build();
await app.Services.EnsureUserAuditDatabaseAsync();

app.MapGet("/health", () => Results.Ok(new { status = "ok", service = "alerts" }));

app.MapGet("/api/v1/alerts", async (UserAuditDbContext db, CancellationToken cancellationToken) =>
{
    var alerts = await db.Alerts
        .Include(x => x.Host)
        .OrderByDescending(x => x.CreatedAtUtc)
        .Take(200)
        .Select(x => new AlertDto(
            x.Id,
            x.Host!.Hostname,
            x.RuleId,
            x.Title,
            x.Severity,
            x.Detail,
            x.CreatedAtUtc,
            x.AcknowledgedAtUtc))
        .ToListAsync(cancellationToken);
    return Results.Ok(alerts);
});

app.MapPost("/api/v1/alerts/{id:guid}/ack", async (
    Guid id,
    HttpContext context,
    IConfiguration configuration,
    UserAuditDbContext db,
    CancellationToken cancellationToken) =>
{
    if (!AdminAuth.IsAuthorized(context.Request.Headers, configuration))
    {
        return Results.Unauthorized();
    }

    var alert = await db.Alerts.FirstOrDefaultAsync(x => x.Id == id, cancellationToken);
    if (alert is null)
    {
        return Results.NotFound();
    }

    alert.AcknowledgedAtUtc = DateTimeOffset.UtcNow;
    await db.SaveChangesAsync(cancellationToken);
    return Results.Ok(new { alert.Id, alert.AcknowledgedAtUtc });
});

app.MapPost("/api/v1/alerts/process", async (
    AlertEngine engine,
    CancellationToken cancellationToken) =>
{
    var created = await engine.ProcessPendingEventsAsync(cancellationToken);
    return Results.Ok(new { created });
});

app.Run();

public sealed class AlertPollingService(IServiceProvider services, ILogger<AlertPollingService> logger) : BackgroundService
{
    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        while (!stoppingToken.IsCancellationRequested)
        {
            try
            {
                await using var scope = services.CreateAsyncScope();
                var engine = scope.ServiceProvider.GetRequiredService<AlertEngine>();
                var created = await engine.ProcessPendingEventsAsync(stoppingToken);
                if (created > 0)
                {
                    logger.LogInformation("Alerts created: {Count}", created);
                }
            }
            catch (Exception ex)
            {
                logger.LogError(ex, "Alert polling failed");
            }

            await Task.Delay(TimeSpan.FromSeconds(30), stoppingToken);
        }
    }
}
