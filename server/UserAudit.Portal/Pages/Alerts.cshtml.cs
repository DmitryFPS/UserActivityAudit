using Microsoft.EntityFrameworkCore;
using UserAudit.Shared.Data;
using UserAudit.Shared.Dtos;

namespace UserAudit.Portal.Pages;

public class AlertsModel(UserAuditDbContext db) : Microsoft.AspNetCore.Mvc.RazorPages.PageModel
{
    public IList<AlertDto> Alerts { get; private set; } = [];

    public async Task OnGetAsync(CancellationToken cancellationToken)
    {
        Alerts = await db.Alerts
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
    }
}
