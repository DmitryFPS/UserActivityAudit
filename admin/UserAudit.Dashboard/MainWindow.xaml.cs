using System.IO;
using System.Windows;
using System.Windows.Threading;
using Microsoft.Win32;
using UserAudit.Admin.Core.Crypto;
using UserAudit.Admin.Core.Display;
using UserAudit.Admin.Core.Forensic;
using UserAudit.Admin.Core.Import;
using UserAudit.Admin.Core.Models;
using UserAudit.Admin.Core.Reports;

namespace UserAudit.Dashboard;

public partial class MainWindow : Window
{
    private readonly EventStore _store = new();
    private readonly LogImporter _importer = new();
    private readonly LocalLogService _localLogs;
    private readonly DispatcherTimer _autoRefreshTimer;
    private int _timelineFilterGeneration;

    public MainWindow()
    {
        InitializeComponent();
        _localLogs = new LocalLogService(_importer);
        LogFolderBox.Text = LocalKeyProvider.DefaultLogsDirectory;
        KeysFolderBox.Text = LocalKeyProvider.DefaultKeysDirectory;
        ReportDatePicker.SelectedDate = DateTime.Today;
        ComputerNameText.Text = $"Компьютер: {Environment.MachineName}";

        _store.Changed += RefreshUi;
        _autoRefreshTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(60) };
        _autoRefreshTimer.Tick += (_, _) => RefreshLogs(silent: true);
        _autoRefreshTimer.Start();

        Loaded += (_, _) => RefreshLogs(silent: false);
    }

    private void RefreshUi()
    {
        var events = _store.GetEvents();
        var alerts = LocalAnalytics.BuildAlerts(events);

        EventCountText.Text = events.Count.ToString();
        AlertCountText.Text = alerts.Count.ToString();
        LastImportText.Text = _store.LastImportUtc is { } ts
            ? $"{ts.ToLocalTime():g} ({_store.LastKeySource ?? "—"})"
            : "ещё не загружалось";

        CategoryBreakdownText.Text = string.Join(" · ",
            events
                .GroupBy(x => x.Category, StringComparer.OrdinalIgnoreCase)
                .OrderByDescending(g => g.Count())
                .Select(g => $"{g.Key}: {g.Count()}"));

        var summary = AuditEventFilter.BuildSummary(events);
        MonitoringSummaryText.Text = "…";

        RefreshTimelineGrid(events, summary);

        AlertsGrid.ItemsSource = alerts
            .Select(x => new
            {
                CreatedAtUtc = x.CreatedAtUtc.ToString("u"),
                x.RuleId,
                x.Title,
                x.Severity,
            })
            .ToList();

        UsbGrid.ItemsSource = events
            .Where(x => x.Category.Equals("usb", StringComparison.OrdinalIgnoreCase))
            .Select(x => new
            {
                TimestampUtc = x.TimestampUtc.ToString("u"),
                x.Action,
                CorrelationId = x.CorrelationId ?? x.Data.GetValueOrDefault("corr") ?? "—",
                VidPid = $"{x.Data.GetValueOrDefault("vid")}:{x.Data.GetValueOrDefault("pid")}",
            })
            .ToList();
    }

    private TimelineFilterOptions ReadTimelineFilters() => new(
        EmployeeMonitoring: EmployeeMonitoringCheck?.IsChecked == true,
        HideNetworkSnapshots: HideNetworkSnapshotsCheck?.IsChecked == true,
        HideProcessEvents: HideProcessCheck?.IsChecked == true,
        HideTamperEvents: HideTamperCheck?.IsChecked == true);

    private void RefreshTimelineGrid(IReadOnlyList<AuditEventModel> events, MonitoringSummary? summary = null)
    {
        var filters = ReadTimelineFilters();
        var generation = Interlocked.Increment(ref _timelineFilterGeneration);
        TimelineFilterStatusText.Text = "Фильтрация…";
        summary ??= AuditEventFilter.BuildSummary(events);

        Task.Run(() =>
        {
            var matched = AuditEventFilter.ApplyTimelineFilter(
                events,
                filters.EmployeeMonitoring,
                filters.HideNetworkSnapshots,
                filters.HideProcessEvents,
                filters.HideTamperEvents).ToList();

            var rows = matched
                .Take(AuditEventFilter.TimelineDisplayLimit)
                .Select(x => new TimelineRow(
                    x.TimestampUtc.ToString("u"),
                    x.Category,
                    x.Action,
                    AuditEventFormatter.Describe(x),
                    x.User ?? string.Empty,
                    x.Severity))
                .ToList();

            var status = AuditEventFilter.FormatTimelineFilterStatus(
                rows.Count,
                matched.Count,
                events.Count);
            var monitoringText = AuditEventFilter.FormatMonitoringSummary(summary, matched.Count);

            Dispatcher.BeginInvoke(() =>
            {
                if (generation != _timelineFilterGeneration)
                {
                    return;
                }

                TimelineGrid.ItemsSource = null;
                TimelineGrid.ItemsSource = rows;
                TimelineFilterStatusText.Text = status;
                MonitoringSummaryText.Text = monitoringText;
            });
        });
    }

    private void TimelineFilter_Changed(object sender, RoutedEventArgs e)
    {
        if (!IsLoaded)
        {
            return;
        }

        RefreshTimelineGrid(_store.GetEvents());
    }

    private void RefreshLogs_Click(object sender, RoutedEventArgs e) => RefreshLogs(silent: false);

    private void RefreshLogs(bool silent)
    {
        _localLogs.LogsDirectory = LogFolderBox.Text.Trim();
        _localLogs.KeysDirectory = KeysFolderBox.Text.Trim();
        _localLogs.ExternalDekFilePath = string.IsNullOrWhiteSpace(DekPathBox.Text)
            ? null
            : DekPathBox.Text.Trim();

        var result = _localLogs.ImportAll();
        if (!result.Success)
        {
            if (!silent)
            {
                MessageBox.Show(this, result.Error, "Импорт логов", MessageBoxButton.OK, MessageBoxImage.Warning);
            }

            SetStatus(result.Error ?? "Ошибка импорта");
            return;
        }

        _store.ReplaceLocalEvents(result.Events, result.KeySource);
        SetStatus($"Загружено событий: {result.Events.Count}");
    }

    private void BrowseLogFolder_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFolderDialog { Title = "Каталог зашифрованных логов" };
        if (dialog.ShowDialog() == true)
        {
            LogFolderBox.Text = dialog.FolderName;
            RefreshLogs(silent: false);
        }
    }

    private void BrowseDek_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFileDialog
        {
            Title = "Файл DEK (32 байта или base64)",
            Filter = "Все файлы|*.*",
        };
        if (dialog.ShowDialog() == true)
        {
            DekPathBox.Text = dialog.FileName;
            RefreshLogs(silent: false);
        }
    }

    private void ExportForensicPack_Click(object sender, RoutedEventArgs e)
    {
        var events = _store.GetEvents();
        if (events.Count == 0)
        {
            MessageBox.Show(this, "Нет событий для экспорта. Сначала обновите логи.", "Forensic pack",
                MessageBoxButton.OK, MessageBoxImage.Information);
            return;
        }

        var dialog = new SaveFileDialog
        {
            Filter = "ZIP|*.zip",
            FileName = $"forensic-{Environment.MachineName}-{DateTime.UtcNow:yyyyMMddHHmmss}.zip",
        };
        if (dialog.ShowDialog() != true)
        {
            return;
        }

        try
        {
            ForensicPackExporter.ExportZip(events, dialog.FileName, Environment.MachineName);
            SetStatus($"Экспорт: {dialog.FileName}");
        }
        catch (Exception ex)
        {
            MessageBox.Show(this, ex.Message, "Ошибка экспорта", MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    private void DailyExcel_Click(object sender, RoutedEventArgs e) => ExportDailyReport(excel: true);
    private void DailyPdf_Click(object sender, RoutedEventArgs e) => ExportDailyReport(excel: false);

    private void ExportDailyReport(bool excel)
    {
        var day = ReportDatePicker.SelectedDate.HasValue
            ? DateOnly.FromDateTime(ReportDatePicker.SelectedDate.Value)
            : DateOnly.FromDateTime(DateTime.Today);
        var summary = DailyActivityReportGenerator.Build(_store.GetEvents(), day);
        var dialog = new SaveFileDialog
        {
            Filter = excel ? "Excel|*.xlsx" : "PDF|*.pdf",
            FileName = excel ? $"activity-{day:yyyy-MM-dd}.xlsx" : $"activity-{day:yyyy-MM-dd}.pdf",
        };
        if (dialog.ShowDialog() != true)
        {
            return;
        }

        if (excel)
        {
            DailyActivityReportGenerator.ExportExcel(summary, dialog.FileName);
        }
        else
        {
            DailyActivityReportGenerator.ExportPdf(summary, dialog.FileName);
        }

        SetStatus($"Отчёт сохранён: {dialog.FileName}");
    }

    private void UsbExcel_Click(object sender, RoutedEventArgs e) => ExportUsbReport(excel: true);
    private void UsbPdf_Click(object sender, RoutedEventArgs e) => ExportUsbReport(excel: false);

    private void ExportUsbReport(bool excel)
    {
        var rows = UsbReportGenerator.Build(_store.GetEvents());
        var dialog = new SaveFileDialog
        {
            Filter = excel ? "Excel|*.xlsx" : "PDF|*.pdf",
            FileName = excel ? "usb-report.xlsx" : "usb-report.pdf",
        };
        if (dialog.ShowDialog() != true)
        {
            return;
        }

        if (excel)
        {
            UsbReportGenerator.ExportExcel(rows, dialog.FileName);
        }
        else
        {
            UsbReportGenerator.ExportPdf(rows, dialog.FileName);
        }

        SetStatus($"USB-отчёт: {dialog.FileName}");
    }

    private void Exit_Click(object sender, RoutedEventArgs e) => Close();

    private void SetStatus(string text) => StatusText.Text = text;

    private sealed record TimelineFilterOptions(
        bool EmployeeMonitoring,
        bool HideNetworkSnapshots,
        bool HideProcessEvents,
        bool HideTamperEvents);

    private sealed record TimelineRow(
        string TimestampUtc,
        string Category,
        string Action,
        string Description,
        string User,
        string Severity);
}
