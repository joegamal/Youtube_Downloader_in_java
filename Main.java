import javax.swing.*;
import java.awt.*;
import java.awt.event.ActionEvent;
import java.io.File;
import java.io.IOException;
import java.net.MalformedURLException;
import java.net.URL;

public class Main {

    public static void main(String[] args) {
        
        // Create the frame
        JFrame frame = new JFrame("Yusuf Gamal YouTube Downloader");
        frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        frame.setSize(450, 200);
        frame.setLayout(new BorderLayout());
        frame.setLocationRelativeTo(null); // Center the window

        // Create a panel to hold components
        JPanel panel = new JPanel(new GridLayout(3, 1, 10, 10));
        panel.setBorder(BorderFactory.createEmptyBorder(20, 20, 20, 20));

        // Create input field
        JTextField urlField = new JTextField();
        panel.add(new JLabel("Enter a YouTube URL:"));
        panel.add(urlField);

        // Create download button
        JButton submitButton = new JButton("Download");
        panel.add(submitButton);

        // Add panel to frame
        frame.add(panel, BorderLayout.CENTER);

        // Status label
        JLabel resultLabel = new JLabel("", SwingConstants.CENTER);
        frame.add(resultLabel, BorderLayout.SOUTH);

        // Action on button click
        submitButton.addActionListener((ActionEvent e) -> {
            String videoUrl = urlField.getText().trim();
            if (videoUrl.isEmpty()) {
                resultLabel.setText("❌ Please enter a URL.");
                return;
            }

            // Validate URL format
            try {
                new URL(videoUrl);
            } catch (MalformedURLException ex) {
                resultLabel.setText("❌ Invalid URL format.");
                return;
            }

            // Set download folder
            String folderPath = "/home/yusufgamal/Videos";

            // Build yt-dlp command
            ProcessBuilder processBuilder = new ProcessBuilder(
                "yt-dlp",
                "-f", "bestvideo+bestaudio",
                "--merge-output-format", "mp4",
                "-o", folderPath + File.separator + "%(title)s.%(ext)s",
                videoUrl
            );

            new Thread(() -> {
                try {
                    resultLabel.setText("⬇️ Downloading...");
                    Process process = processBuilder.start();
                    int exitCode = process.waitFor();
                    if (exitCode == 0) {
                        SwingUtilities.invokeLater(() ->
                            resultLabel.setText("✅ Download completed!")
                        );
                    } else {
                        SwingUtilities.invokeLater(() ->
                            resultLabel.setText("❌ Failed to download video.")
                        );
                    }
                } catch (IOException | InterruptedException ex) {
                    ex.printStackTrace();
                    SwingUtilities.invokeLater(() ->
                        resultLabel.setText("⚠️ Error: " + ex.getMessage())
                    );
                }
            }).start();
        });

        // Show the window
        frame.setVisible(true);
    }
}
