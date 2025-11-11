import 'dart:io';

String extractRepoFromUrl(String url) {
  if (url.contains('@')) {
    final regex = RegExp(r':([^/]+/[^/]+?)(?:\.git)?$');
    final match = regex.firstMatch(url);
    return match?.group(1) ?? url;
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
    return match?.group(1) ?? url;
  }
  return url;
}

Future<String> getDownloadDirectory(String? path) async {
  if (path == null) {
    return Directory.current.path;
  } else {
    return path;
  }
}
