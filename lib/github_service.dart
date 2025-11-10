import 'dart:async';
import 'dart:convert';
import 'dart:io';

class GitHubRelease {
  final String tagName;
  final List<GitHubAsset> assets;

  GitHubRelease({required this.tagName, required this.assets});

  factory GitHubRelease.fromJson(Map<String, dynamic> json) {
    final assets = <GitHubAsset>[];
    if (json['assets'] != null) {
      for (final asset in json['assets']) {
        assets.add(GitHubAsset.fromJson(asset));
      }
    }

    return GitHubRelease(tagName: json['tag_name'] ?? '', assets: assets);
  }
}

class GitHubAsset {
  final String name;
  final String browserDownloadUrl;

  GitHubAsset({required this.name, required this.browserDownloadUrl});

  factory GitHubAsset.fromJson(Map<String, dynamic> json) {
    return GitHubAsset(
      name: json['name'] ?? '',
      browserDownloadUrl: json['browser_download_url'] ?? '',
    );
  }

  Map<String, String> toFileInfo({String? mirrorUrl}) {
    String downloadUrl = browserDownloadUrl;
    if (mirrorUrl != null) {
      downloadUrl = '$mirrorUrl$downloadUrl';
    }
    return {'name': name, 'url': downloadUrl};
  }
}

class GitHubService {
  final HttpClient _client = HttpClient();

  Future<List<GitHubRelease>> getReleases(String repo) async {
    try {
      print('获取发布信息...');
      final request = await _client.getUrl(
        Uri.parse('https://api.github.com/repos/$repo/releases'),
      );
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
    } catch (e) {
      print('获取发布信息失败: $e');
      rethrow;
    }
  }

  Future<String> getLatestRelease(String repo) async {
    try {
      final request = await _client.getUrl(
        Uri.parse('https://api.github.com/repos/$repo/releases/latest'),
      );
      final response = await request.close();

      if (response.statusCode != HttpStatus.ok) {
        throw HttpException('获取发布信息失败，状态码: ${response.statusCode}');
      }

      final responseBody = await response.transform(utf8.decoder).join();
      final Map<String, dynamic> data = json.decode(responseBody);

      if (data.isEmpty) {
        throw Exception('没有找到发布信息');
      }

      return data['tag_name'];
    } catch (e) {
      print('获取发布信息失败: $e');
      rethrow;
    }
  }

  void close() {
    _client.close();
  }
}
