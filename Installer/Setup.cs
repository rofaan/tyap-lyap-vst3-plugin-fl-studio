using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Reflection;
using System.Text.RegularExpressions;
using System.Windows.Forms;

[assembly: AssemblyTitle("Tyap Lyap VST3 Setup")]
[assembly: AssemblyProduct("Tyap Lyap")]
[assembly: AssemblyDescription("Tyap Lyap 2.2 VST3 setup and updater for Windows x64")]
[assembly: AssemblyVersion("2.2.0.0")]
[assembly: AssemblyFileVersion("2.2.0.0")]

internal static class Setup
{
    private const string BundleName = "Tyap Lyap.vst3";
    private const string Caption = "Тяп Ляп 2.2";

    [STAThread]
    private static int Main()
    {
        Application.EnableVisualStyles();
        try
        {
            string common = Environment.GetFolderPath(Environment.SpecialFolder.CommonProgramFiles);
            if (String.IsNullOrEmpty(common) || !Path.IsPathRooted(common))
                throw new IOException("Windows не вернула путь к Common Files.");
            string target = TargetPath(common);
            bool update = HasInstalledBundle(common);
            if (! Confirm(target, update))
                return 0;

            EnsureFLClosed();
            string backup = Install(common);
            string message = (backup == null ? "Плагин установлен." : "Плагин обновлён до 2.2.")
                + "\n\nОткрой FL Studio. Если плагин не появился, "
                + "выбери Options → Manage plugins → Find installed plugins.";
            if (backup != null)
                message += "\n\nПредыдущая версия сохранена здесь:\n" + backup;
            MessageBox.Show(message, Caption, MessageBoxButtons.OK, MessageBoxIcon.Information);
            return 0;
        }
        catch (Exception error)
        {
            MessageBox.Show(error.Message, Caption, MessageBoxButtons.OK, MessageBoxIcon.Error);
            return 1;
        }
    }

    private static string TargetPath(string common)
    {
        return Path.Combine(common, "VST3", BundleName);
    }

    internal static bool HasInstalledBundle(string common)
    {
        string target = TargetPath(common);
        return Directory.Exists(target) || File.Exists(target);
    }

    private static string InstalledVersion(string target)
    {
        try
        {
            string manifest = Path.Combine(target, "Contents", "Resources", "moduleinfo.json");
            if (File.Exists(manifest) && new FileInfo(manifest).Length <= 65536)
            {
                Match match = Regex.Match(File.ReadAllText(manifest), "\"Version\"\\s*:\\s*\"([0-9.]+)\"");
                if (match.Success)
                    return " " + match.Groups[1].Value;
            }
        }
        catch (IOException) {}
        catch (UnauthorizedAccessException) {}
        return "";
    }

    private static bool Confirm(string target, bool update)
    {
        using (var window = CreateConfirmation(target, update))
            return window.ShowDialog() == DialogResult.OK;
    }

    internal static Form CreateConfirmation(string target, bool update)
    {
        var window = new Form();
        window.Text = Caption + " · VST3 для Windows x64";
        window.StartPosition = FormStartPosition.CenterScreen;
        window.FormBorderStyle = FormBorderStyle.FixedDialog;
        window.AutoScaleDimensions = new SizeF(96, 96);
        window.AutoScaleMode = AutoScaleMode.Dpi;
        window.MaximizeBox = false;
        window.MinimizeBox = false;
        window.Font = new Font("Segoe UI", 10.0f);
        window.ClientSize = new Size(640, 228);

        var banner = new PictureBox();
        banner.SetBounds(0, 0, 640, 37);
        banner.Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right;
        banner.SizeMode = PictureBoxSizeMode.Zoom;
        banner.BackColor = Color.FromArgb(20, 10, 6);
        banner.TabStop = false;
        banner.AccessibleName = "Тяп Ляп · Street Spatial Processor";
        using (Stream input = typeof(Setup).Assembly.GetManifestResourceStream("TyapLyap.Banner"))
        {
            if (input == null) throw new IOException("В установщике не найдена плашка.");
            using (Image original = Image.FromStream(input))
                banner.Image = new Bitmap(original);
        }
        window.Disposed += delegate { banner.Image.Dispose(); };
        window.Controls.Add(banner);

        var summary = new Label();
        summary.SetBounds(18, 57, 604, 58);
        summary.Text = update
            ? "Найдена «Тяп Ляп»" + InstalledVersion(target)
                + ". Обновление заменит VST3 и сохранит копию предыдущей версии."
            : "Установи «Тяп Ляп» 2.2 в системную папку VST3.";
        window.Controls.Add(summary);

        var path = new Label();
        path.SetBounds(18, 119, 604, 32);
        path.Text = target;
        path.AutoEllipsis = true;
        window.Controls.Add(path);

        var proceed = new Button();
        proceed.Text = update ? "Обновить до 2.2" : "Установить";
        proceed.SetBounds(331, 176, 180, 34);
        proceed.DialogResult = DialogResult.OK;
        proceed.TabIndex = 0;
        window.Controls.Add(proceed);

        var cancel = new Button();
        cancel.Text = "Отмена";
        cancel.SetBounds(519, 176, 103, 34);
        cancel.DialogResult = DialogResult.Cancel;
        cancel.TabIndex = 1;
        window.Controls.Add(cancel);
        window.AcceptButton = proceed;
        window.CancelButton = cancel;
        return window;
    }

    private static void EnsureFLClosed()
    {
        foreach (var process in Process.GetProcesses())
            using (process)
            {
                string name;
                try { name = process.ProcessName; }
                catch (InvalidOperationException) { continue; }
                if (name.Equals("FL", StringComparison.OrdinalIgnoreCase)
                    || name.StartsWith("FL64", StringComparison.OrdinalIgnoreCase))
                    throw new InvalidOperationException("Закрой FL Studio и запусти установщик снова.");
            }
    }

    // Stage the full bundle first. Backups are outside the VST3 scan directory.
    internal static string Install(string commonFiles)
    {
        var vstDirectory = Directory.CreateDirectory(Path.Combine(commonFiles, "VST3"));
        var backups = Directory.CreateDirectory(Path.Combine(commonFiles, "Tyap Lyap Backups"));
        RejectLink(vstDirectory);
        RejectLink(backups);
        string target = Path.Combine(vstDirectory.FullName, BundleName);
        bool oldDirectory = Directory.Exists(target);
        bool oldFile = File.Exists(target);
        if (oldDirectory) RejectLink(new DirectoryInfo(target));
        if (oldFile) RejectLink(new FileInfo(target));

        string id = DateTime.UtcNow.ToString("yyyyMMdd-HHmmss") + "-" + Guid.NewGuid().ToString("N");
        string stage = Path.Combine(backups.FullName, "stage-" + id);
        string backup = Path.Combine(backups.FullName, id + ".backup");
        Directory.CreateDirectory(stage);
        bool movedOld = false;
        try
        {
            Extract("TyapLyap.Binary", Path.Combine(stage, "Contents", "x86_64-win", BundleName));
            Extract("TyapLyap.ModuleInfo", Path.Combine(stage, "Contents", "Resources", "moduleinfo.json"));
            if (oldDirectory)
                Directory.Move(target, backup);
            else if (oldFile)
                File.Move(target, backup);
            movedOld = oldDirectory || oldFile;
            Directory.Move(stage, target);
            return movedOld ? backup : null;
        }
        catch (Exception installError)
        {
            if (movedOld)
            {
                try
                {
                    if (oldDirectory)
                        Directory.Move(backup, target);
                    else
                        File.Move(backup, target);
                }
                catch (Exception rollbackError)
                {
                    throw new IOException("Не удалось вернуть предыдущую версию. Её копия находится здесь:\n"
                        + backup + "\n\n" + rollbackError.Message, installError);
                }
            }
            throw new IOException("Установка не завершена. Закрой FL Studio и повтори попытку.\n\n"
                + installError.Message, installError);
        }
        finally
        {
            // The uniquely named staging path is never the installed bundle or backup.
            try { if (Directory.Exists(stage)) Directory.Delete(stage, true); }
            catch (IOException) {}
            catch (UnauthorizedAccessException) {}
        }
    }

    private static void RejectLink(FileSystemInfo entry)
    {
        if ((entry.Attributes & FileAttributes.ReparsePoint) != 0)
            throw new IOException("Установщик не может заменить ссылку:\n" + entry.FullName);
    }

    private static void Extract(string name, string destination)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(destination));
        using (Stream input = typeof(Setup).Assembly.GetManifestResourceStream(name))
        {
            if (input == null || input.Length == 0)
                throw new IOException("Установщик повреждён. Не найден компонент " + name);
            using (var output = new FileStream(destination, FileMode.CreateNew, FileAccess.Write))
                input.CopyTo(output);
        }
    }
}
