import 'dart:io';

String extractRepoFromUrl(String url) {
  if (url.contains('@')) {
    final regex = RegExp(r':([^/]+/[^/]+?)(?:\.git)?$');
    final match = regex.firstMatch(url);
    return match?.group(1) ?? '';
  }

  try {
    final uri = Uri.parse(url);
    final pathSegments = uri.pathSegments
        .where((segment) => segment.isNotEmpty)
        .toList();

    if (pathSegments.length >= 2) {
      String user = pathSegments[0];
      String repo = pathSegments[1].replaceAll('.git', '');
      return '$user/$repo';
    }
  } catch (e) {
    final regex = RegExp(r'github\.com[/:]([^/]+/[^/]+?)(?:\.git)?$');
    final match = regex.firstMatch(url);
    return match?.group(1) ?? '';
  }
  return url;
}

Future<String> getDownloadDirectory(String? path) async {
  if (path == null) {
    // const winlatorPath =
    // '/data/data/com.winlator/files/installed_components/box64';
    // final winlatorDir = Directory(winlatorPath);
    // bool exists = false;
    // try {
    // exists = await winlatorDir.exists();
    // } catch (e) {
    // return Directory.current.path;
    // }
    // if (exists) {
    // return winlatorPath;
    // } else {
    // return Directory.current.path;
    // }
    return Directory.current.path;
  } else {
    return path;
  }
}

String createProgressBar(int received, int total) {
  const width = 20;
  if (total <= 0) return '[' + '?' * width + ']';
  final progress = received / total;
  final filled = (progress * width).round();
  final empty = width - filled;
  return '[' + '=' * filled + ' ' * empty + ']';
}

String formatBytes(int bytes) {
  if (bytes < 1024) return '$bytes B';
  if (bytes < 1024 * 1024) return '${(bytes / 1024).toStringAsFixed(1)} KB';
  if (bytes < 1024 * 1024 * 1024)
    return '${(bytes / 1024 / 1024).toStringAsFixed(1)} MB';
  return '${(bytes / 1024 / 1024 / 1024).toStringAsFixed(1)} GB';
}

String formatTime(double seconds) {
  if (seconds.isInfinite || seconds.isNaN || seconds <= 0) return '--:--';
  final mins = (seconds / 60).floor();
  final secs = (seconds % 60).floor();
  return '${mins.toString().padLeft(2, '0')}:${secs.toString().padLeft(2, '0')}';
}
