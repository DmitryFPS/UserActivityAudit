using Microsoft.EntityFrameworkCore;
using UserAudit.Shared.Data;
using UserAudit.Shared.Dtos;

namespace UserAudit.Portal.Pages;

public class TimelineModel(UserAuditDbContext db) : Microsoft.AspNetCore.Mvc.RazorPages.PageModel
{
    public IList<TimelineEventDto> Events { get; private set; } = [];

    public async Task OnGetAsync(CancellationToken cancellationToken)
    {
        Events = await db.AuditEvents
            .Include(x => x.Host)
            .OrderByDescending(x => x.TimestampUtc)
            .Take(200)
            .Select(x => new TimelineEventDto(
                x.EventId,
                x.Host!.Hostname,
                x.TimestampUtc,
                x.Category,
                x.Action,
                x.Severity,
                x.PayloadJson))
            .ToListAsync(cancellationToken);
    }
}
