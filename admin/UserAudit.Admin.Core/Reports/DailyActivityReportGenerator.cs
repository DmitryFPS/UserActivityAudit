using ClosedXML.Excel;
using QuestPDF.Fluent;
using QuestPDF.Helpers;
using QuestPDF.Infrastructure;
using UserAudit.Admin.Core.Models;

namespace UserAudit.Admin.Core.Reports;

public static class DailyActivityReportGenerator
{
    public static DailyActivitySummary Build(IEnumerable<AuditEventModel> events, DateOnly day)
    {
        var dayEvents = events
            .Where(x => DateOnly.FromDateTime(x.TimestampUtc.UtcDateTime) == day)
            .ToList();

        var logins = dayEvents.Count(x =>
            x.Category.Equals("session", StringComparison.OrdinalIgnoreCase) &&
            x.Action.Equals("login", StringComparison.OrdinalIgnoreCase));

        var topApps = dayEvents
            .Where(x => x.Category.Equals("process", StringComparison.OrdinalIgnoreCase) &&
                        x.Action.Equals("start", StringComparison.OrdinalIgnoreCase))
            .Select(x => x.Data.TryGetValue("path", out var path) ? Path.GetFileName(path) :
                         x.Data.TryGetValue("exe", out var exe) ? exe :
                         x.Data.TryGetValue("image_name", out var image) ? image :
                         x.Data.TryGetValue("name", out var name) ? name : "unknown")
            .GroupBy(x => x, StringComparer.OrdinalIgnoreCase)
            .OrderByDescending(g => g.Count())
            .Take(15)
            .Select(g => new TopAppRow(g.Key, g.Count()))
            .ToList();

        var focusMinutes = EstimateFocusMinutes(dayEvents);

        return new DailyActivitySummary(day, logins, focusMinutes, topApps, dayEvents.Count);
    }

    public static void ExportExcel(DailyActivitySummary summary, string outputPath)
    {
        using var workbook = new XLWorkbook();
        var sheet = workbook.Worksheets.Add("Daily Activity");
        sheet.Cell(1, 1).Value = "Date";
        sheet.Cell(1, 2).Value = summary.Day.ToString("yyyy-MM-dd");
        sheet.Cell(2, 1).Value = "Logins";
        sheet.Cell(2, 2).Value = summary.LoginCount;
        sheet.Cell(3, 1).Value = "Focus minutes (est.)";
        sheet.Cell(3, 2).Value = summary.FocusMinutesEstimate;
        sheet.Cell(4, 1).Value = "Total events";
        sheet.Cell(4, 2).Value = summary.TotalEvents;

        sheet.Cell(6, 1).Value = "Application";
        sheet.Cell(6, 2).Value = "Starts";
        var row = 7;
        foreach (var app in summary.TopApplications)
        {
            sheet.Cell(row, 1).Value = app.Name;
            sheet.Cell(row, 2).Value = app.StartCount;
            row++;
        }

        sheet.Columns().AdjustToContents();
        workbook.SaveAs(outputPath);
    }

    public static void ExportPdf(DailyActivitySummary summary, string outputPath)
    {
        QuestPDF.Settings.License = LicenseType.Community;
        Document.Create(container =>
        {
            container.Page(page =>
            {
                page.Margin(30);
                page.Header().Text($"Daily Activity — {summary.Day:yyyy-MM-dd}").FontSize(18).SemiBold();
                page.Content().Column(col =>
                {
                    col.Item().Text($"Logins: {summary.LoginCount}");
                    col.Item().Text($"Focus minutes (est.): {summary.FocusMinutesEstimate}");
                    col.Item().Text($"Total events: {summary.TotalEvents}");
                    col.Item().PaddingTop(10).Text("Top applications").SemiBold();
                    foreach (var app in summary.TopApplications)
                    {
                        col.Item().Text($"{app.Name}: {app.StartCount}");
                    }
                });
            });
        }).GeneratePdf(outputPath);
    }

    private static int EstimateFocusMinutes(IReadOnlyList<AuditEventModel> dayEvents)
    {
        var focusEvents = dayEvents
            .Where(x => x.Category.Equals("window", StringComparison.OrdinalIgnoreCase) &&
                        x.Action.Equals("focus", StringComparison.OrdinalIgnoreCase))
            .OrderBy(x => x.TimestampUtc)
            .ToList();

        if (focusEvents.Count < 2)
        {
            return focusEvents.Count * 5;
        }

        var minutes = 0;
        for (var i = 1; i < focusEvents.Count; i++)
        {
            var delta = focusEvents[i].TimestampUtc - focusEvents[i - 1].TimestampUtc;
            minutes += (int)Math.Clamp(delta.TotalMinutes, 1, 30);
        }

        return minutes;
    }
}

public sealed record DailyActivitySummary(
    DateOnly Day,
    int LoginCount,
    int FocusMinutesEstimate,
    IReadOnlyList<TopAppRow> TopApplications,
    int TotalEvents);

public sealed record TopAppRow(string Name, int StartCount);
