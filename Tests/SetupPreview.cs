using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Windows.Forms;

internal static class SetupPreview
{
    [STAThread]
    private static void Main(string[] args)
    {
        Application.EnableVisualStyles();
        // Offscreen render of the same form used by the shipped installer. Never installs files.
        using (var form = Setup.CreateConfirmation(@"C:\Program Files\Common Files\VST3\Tyap Lyap.vst3", true))
        {
            form.ShowInTaskbar = false;
            form.Opacity = 0;
            form.Show();
            form.PerformLayout();
            form.Update();
            using (var image = new Bitmap(form.Width, form.Height))
            {
                form.DrawToBitmap(image, new Rectangle(0, 0, image.Width, image.Height));
                image.Save(args[0], ImageFormat.Png);
            }
            form.Close();
        }
    }
}
