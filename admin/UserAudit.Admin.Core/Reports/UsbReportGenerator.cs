using ClosedXML.Excel;
using QuestPDF.Fluent;
using QuestPDF.Infrastructure;
using UserAudit.Admin.Core.Models;

namespace UserAudit.Admin.Core.Reports;

public static class UsbReportGenerator
{
    public static IReadOnlyList<UsbEventRow> Build(IEnumerable<AuditEventModel> events)
    {
        return events
            .Where(x => x.Category.Equals("usb", StringComparison.OrdinalIgnoreCase))
            .OrderByDescending(x => x.TimestampUtc)
            .Select(x => new UsbEventRow(
                x.TimestampUtc,
                x.Host,
                x.Action,
                x.CorrelationId ?? x.Data.GetValueOrDefault("corr"),
                x.Data.GetValueOrDefault("vid"),
                x.Data.GetValueOrDefault("pid"),
                x.Data.GetValueOrDefault("serial"),
                x.Data.GetValueOrDefault("label")))
            .ToList();
    }

    public static void ExportExcel(IReadOnlyList<UsbEventRow> rows, string outputPath)
    {
        using var workbook = new XLWorkbook();
        var sheet = workbook.Worksheets.Add("USB");
        sheet.Cell(1, 1).Value = "Time (UTC)";
        sheet.Cell(1, 2).Value = "Host";
        sheet.Cell(1, 3).Value = "Action";
        sheet.Cell(1, 4).Value = "Correlation ID";
        sheet.Cell(1, 5).Value = "VID";
        sheet.Cell(1, 6).Value = "PID";
        sheet.Cell(1, 7).Value = "Serial";
        sheet.Cell(1, 8).Value = "Label";

        var rowIndex = 2;
        foreach (var row in rows)
        {
            sheet.Cell(rowIndex, 1).Value = row.TimestampUtc.UtcDateTime;
            sheet.Cell(rowIndex, 2).Value = row.Host;
            sheet.Cell(rowIndex, 3).Value = row.Action;
            sheet.Cell(rowIndex, 4).Value = row.CorrelationId ?? string.Empty;
            sheet.Cell(rowIndex, 5).Value = row.Vid ?? string.Empty;
            sheet.Cell(rowIndex, 6).Value = row.Pid ?? string.Empty;
            sheet.Cell(rowIndex, 7).Value = row.Serial ?? string.Empty;
            sheet.Cell(rowIndex, 8).Value = row.Label ?? string.Empty;
            rowIndex++;
        }

        sheet.Columns().AdjustToContents();
        workbook.SaveAs(outputPath);
    }

    public static void ExportPdf(IReadOnlyList<UsbEventRow> rows, string outputPath)
    {
        QuestPDF.Settings.License = LicenseType.Community;
        Document.Create(container =>
        {
            container.Page(page =>
            {
                page.Margin(30);
                page.Header().Text("USB Activity Report").FontSize(18).SemiBold();
                page.Content().Column(col =>
                {
                    foreach (var row in rows.Take(200))
                    {
                        col.Item().Text(
                            $"{row.TimestampUtc:u} | {row.Host} | {row.Action} | corr={row.CorrelationId ?? "-"} | {row.Vid}:{row.Pid}");
                    }
                });
            });
        }).GeneratePdf(outputPath);
    }
}

public sealed record UsbEventRow(
    DateTimeOffset TimestampUtc,
    string Host,
    string Action,
    string? CorrelationId,
    string? Vid,
    string? Pid,
    string? Serial,
    string? Label);
