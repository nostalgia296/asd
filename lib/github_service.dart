import 'dart:async';
import 'dart:convert';
import 'dart:io';

class GitHubRelease {
  final String tagName;
  final List<GitHubAsset> assets;

  const GitHubRelease({required this.tagName, required this.assets});

  factory GitHubRelease.fromJson(Map<String, dynamic> json) {
    final assets = <GitHubAsset>[];
    final assetsJson = json['assets'] as List<dynamic>?;

    if (assetsJson != null) {
      for (final asset in assetsJson) {
        assets.add(GitHubAsset.fromJson(asset as Map<String, dynamic>));
      }
    }

    return GitHubRelease(
      tagName: json['tag_name'] as String? ?? '',
      assets: assets,
    );
  }
}

class GitHubAsset {
  final String name;
  final String browserDownloadUrl;

  const GitHubAsset({required this.name, required this.browserDownloadUrl});

  factory GitHubAsset.fromJson(Map<String, dynamic> json) {
    return GitHubAsset(
      name: json['name'] as String? ?? '',
      browserDownloadUrl: json['browser_download_url'] as String? ?? '',
    );
  }

  Map<String, String> toFileInfo({String? mirrorUrl}) {
    String downloadUrl = browserDownloadUrl;
    if (mirrorUrl != null) {
      if (!downloadUrl.startsWith(mirrorUrl)) {
        downloadUrl = '$mirrorUrl$downloadUrl';
      }
    }
    return {'name': name, 'url': downloadUrl};
  }
}

class GitHubService {
  static const String _apiBaseUrl = 'https://api.github.com';
  static const Duration _timeout = Duration(seconds: 30);

  final HttpClient _client;

  GitHubService() : _client = _createHttpClient();

  static HttpClient _createHttpClient() {
    final client = HttpClient()
      ..connectionTimeout = _timeout
      ..idleTimeout = _timeout
      ..userAgent = 'GitHub-Release-Downloader/1.0'
      ..autoUncompress = true;
    return client;
  }

  Future<List<GitHubRelease>> getReleases(String repo) async {
    try {
      print('获取发布信息...');
      final uri = Uri.parse('$_apiBaseUrl/repos/$repo/releases');
      final request = await _client.getUrl(uri);
      final response = await request.close();

      if (response.statusCode != HttpStatus.ok) {
        throw HttpException('获取发布信息失败，状态码: ${response.statusCode}');
      }

      final responseBody = await response.transform(utf8.decoder).join();
      final List<dynamic> data = json.decode(responseBody);

      if (data.isEmpty) {
        print('没有找到tag');
        return [];
      }

      return data
          .map((json) => GitHubRelease.fromJson(json as Map<String, dynamic>))
          .toList();
    } on HttpException {
      rethrow;
    } on FormatException {
      print('响应格式错误');
      rethrow;
    } catch (e) {
      print('获取发布信息失败: $e');
      rethrow;
    }
  }

  Future<String> getLatestRelease(String repo) async {
    try {
      final uri = Uri.parse('$_apiBaseUrl/repos/$repo/releases/latest');
      final request = await _client.getUrl(uri);
      final response = await request.close();

      if (response.statusCode != HttpStatus.ok) {
        throw HttpException('获取最新发布信息失败，状态码: ${response.statusCode}');
      }

      final responseBody = await response.transform(utf8.decoder).join();
      final Map<String, dynamic> data = json.decode(responseBody);

      if (data.isEmpty) {
        throw Exception('没有找到发布信息');
      }

      return data['tag_name'] as String? ?? '';
    } on HttpException {
      rethrow;
    } on FormatException {
      print('响应格式错误');
      rethrow;
    } catch (e) {
      print('获取最新发布信息失败: $e');
      rethrow;
    }
  }

  void close() {
    _client.close();
  }
}
