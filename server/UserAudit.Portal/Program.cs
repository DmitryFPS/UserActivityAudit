using Microsoft.EntityFrameworkCore;
using UserAudit.Shared;
using UserAudit.Shared.Data;
using UserAudit.Shared.Dtos;

var builder = WebApplication.CreateBuilder(args);

var connectionString = builder.Configuration.GetConnectionString("Default")
    ?? builder.Configuration["UserAudit:Database:ConnectionString"]
    ?? "Host=localhost;Port=5432;Database=useraudit;Username=useraudit;Password=useraudit";

builder.Services.AddUserAuditDatabase(connectionString);
builder.Services.AddRazorPages();

var app = builder.Build();
await app.Services.EnsureUserAuditDatabaseAsync();

if (!app.Environment.IsDevelopment())
{
    app.UseExceptionHandler("/Error");
}

app.UseStaticFiles();
app.UseRouting();
app.MapRazorPages();
app.MapGet("/health", () => Results.Ok(new { status = "ok", service = "portal" }));

app.MapGet("/api/v1/portal/timeline", async (UserAuditDbContext db, CancellationToken cancellationToken) =>
{
    var events = await db.AuditEvents
        .Include(x => x.Host)
        .OrderByDescending(x => x.TimestampUtc)
        .Take(500)
        .Select(x => new TimelineEventDto(
            x.EventId,
            x.Host!.Hostname,
            x.TimestampUtc,
            x.Category,
            x.Action,
            x.Severity,
            x.PayloadJson))
        .ToListAsync(cancellationToken);
    return Results.Ok(events);
});

app.Run();
