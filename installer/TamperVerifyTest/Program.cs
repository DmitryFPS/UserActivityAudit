using UserAudit.Admin.Core.Crypto;
using UserAudit.Admin.Core.Import;

static void TakeownGrant(string path)
{
    var psi1 = new System.Diagnostics.ProcessStartInfo("takeown", $"/F \"{path}\" /A")
    {
        UseShellExecute = false,
        CreateNoWindow = true,
    };
    using (var p1 = System.Diagnostics.Process.Start(psi1))
    {
        p1?.WaitForExit(5000);
    }

    var psi2 = new System.Diagnostics.ProcessStartInfo("icacls", $"\"{path}\" /grant *S-1-5-32-544:F")
    {
        UseShellExecute = false,
        CreateNoWindow = true,
    };
    using (var p2 = System.Diagnostics.Process.Start(psi2))
    {
        p2?.WaitForExit(5000);
    }
}

if (!OperatingSystem.IsWindows())
{
    Console.Error.WriteLine("TamperVerifyTest: Windows only.");
    return 1;
}

var logsDir = LocalKeyProvider.DefaultLogsDirectory;
var keysDir = LocalKeyProvider.DefaultKeysDirectory;
if (!LocalKeyProvider.TryLoadDek(keysDir, out var dek, out var keyError))
{
    Console.Error.WriteLine($"FAIL: {keyError}");
    return 1;
}

var encFiles = Directory.GetFiles(logsDir, "*.jsonl.enc", SearchOption.TopDirectoryOnly)
    .OrderBy(x => x, StringComparer.OrdinalIgnoreCase)
    .ToList();
if (encFiles.Count == 0)
{
    Console.Error.WriteLine("FAIL: no .jsonl.enc files");
    return 1;
}

var encPath = encFiles[^1];
var allLines = File.ReadAllLines(encPath).Where(x => !string.IsNullOrWhiteSpace(x)).ToList();
if (allLines.Count < 2)
{
    Console.Error.WriteLine("FAIL: need at least 2 encrypted lines");
    return 1;
}

var lines = allLines.Take(Math.Min(5, allLines.Count)).ToList();
var targetIndex = 1;
if (!EncryptedLogReader.TryDecryptLine(dek, lines[targetIndex].Trim(), out var plaintext))
{
    Console.Error.WriteLine("FAIL: could not decrypt target line");
    return 1;
}

const string hmacKey = "\"hmac\":\"";
var hmacPos = plaintext.LastIndexOf(hmacKey, StringComparison.Ordinal);
if (hmacPos < 0)
{
    Console.Error.WriteLine("FAIL: hmac field not found");
    return 1;
}

var tampered = plaintext.ToCharArray();
var flipIndex = hmacPos + hmacKey.Length;
tampered[flipIndex] = tampered[flipIndex] == '0' ? '1' : '0';
lines[targetIndex] = EncryptedLogReader.EncryptLine(dek, new string(tampered));

var backup = Path.Combine(Path.GetTempPath(), $"ua-tamper-{Guid.NewGuid():N}.bak");
File.Copy(encPath, backup, overwrite: true);
try
{
    TakeownGrant(encPath);
    File.WriteAllLines(encPath, lines);

    var agent = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "UserAudit", "UserAudit.exe");
    if (!File.Exists(agent))
    {
        agent = Path.GetFullPath(Path.Combine(
            AppContext.BaseDirectory, "..", "..", "..", "..", "..", "build", "native", "UserAuditSvc", "Release", "UserAudit.exe"));
    }

    if (!File.Exists(agent))
    {
        Console.Error.WriteLine($"FAIL: UserAudit.exe not found at {agent}");
        return 1;
    }

    var psi = new System.Diagnostics.ProcessStartInfo
    {
        FileName = agent,
        Arguments = "--decrypt --verify",
        RedirectStandardOutput = true,
        RedirectStandardError = true,
        UseShellExecute = false,
        CreateNoWindow = true,
    };
    using var proc = System.Diagnostics.Process.Start(psi)!;
    var stdoutTask = proc.StandardOutput.ReadToEndAsync();
    var stderrTask = proc.StandardError.ReadToEndAsync();
    if (!proc.WaitForExit(120000))
    {
        proc.Kill(true);
        Console.Error.WriteLine("FAIL: verify timeout");
        return 1;
    }

    var stderr = stderrTask.GetAwaiter().GetResult();
    _ = stdoutTask.GetAwaiter().GetResult();
    Console.WriteLine($"verify exit={proc.ExitCode}");
    if (!string.IsNullOrWhiteSpace(stderr))
    {
        Console.WriteLine(stderr.Trim());
    }

    return proc.ExitCode == 2 ? 0 : 2;
}
finally
{
    TakeownGrant(encPath);
    File.Copy(backup, encPath, overwrite: true);
    File.Delete(backup);
}
