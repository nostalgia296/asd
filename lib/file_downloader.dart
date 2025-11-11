import 'dart:async';
import 'dart:io';
import 'package:pool/pool.dart';

class DownloadResult {
  final int successCount;
  final int failureCount;
  final List<DownloadFailure> failures;

  DownloadResult({
    required this.successCount,
    required this.failureCount,
    required this.failures,
  });
}

class DownloadFailure {
  final String fileName;
  final String error;

  DownloadFailure({required this.fileName, required this.error});
}

class FileDownloader {
  Future<DownloadResult> downloadFilesConcurrently(
    List<Map<String, String>> files,
    String downloadDir,
    bool forceOverwrite,
  ) async {
    print('\n开始下载 ${files.length} 个文件...');

    final failures = <DownloadFailure>[];
    final pool = Pool(5);

    final completer = Completer<void>();
    final progress = _DownloadProgress(files.length);

    final progressStream = Stream.periodic(const Duration(milliseconds: 200), (
      _,
    ) {
      if (files.length > 1) {
        print(
          '\r下载进度: ${progress.completed}/${files.length} | 成功: ${progress.success} | 失败: ${progress.failures}     ',
        );
      }
    }).takeWhile((_) => !completer.isCompleted);

    progressStream.listen((_) {});

    final tasks = files.map((fileInfo) async {
      final resource = await pool.request();

      try {
        final fileName = fileInfo['name']!;
        final downloadUrl = fileInfo['url']!;
        final outputPath = '$downloadDir${Platform.pathSeparator}$fileName';

        if (files.length == 1) {
          await downloadFileWithProgress(
            downloadUrl,
            outputPath,
            fileName,
            forceOverwrite,
          );
        } else {
          await downloadFileSimple(
            downloadUrl,
            outputPath,
            fileName,
            forceOverwrite,
          );
        }
        progress.incrementSuccess();
      } catch (e) {
        final fileName = fileInfo['name']!;
        failures.add(DownloadFailure(fileName: fileName, error: e.toString()));
        progress.incrementFailure();
      } finally {
        progress.incrementCompleted();
        resource.release();
      }
    }).toList();

    await Future.wait(tasks);
    completer.complete();

    return DownloadResult(
      successCount: progress.success,
      failureCount: progress.failures,
      failures: failures,
    );
  }

  Future<void> downloadFileWithProgress(
    String url,
    String outputPath,
    String fileName,
    bool forceOverwrite,
  ) async {
    return _downloadFile(
      url,
      outputPath,
      fileName,
      forceOverwrite,
      showProgress: true,
    );
  }

  Future<void> downloadFileSimple(
    String url,
    String outputPath,
    String fileName,
    bool forceOverwrite,
  ) async {
    return _downloadFile(
      url,
      outputPath,
      fileName,
      forceOverwrite,
      showProgress: false,
    );
  }

  HttpClient _createHttpClient() {
    final client = HttpClient();

    client.connectionTimeout = const Duration(seconds: 30);

    client.idleTimeout = const Duration(seconds: 60);

    client.userAgent = 'FileDownloader/1.0';

    client.autoUncompress = true;

    return client;
  }

  Future<void> _downloadFile(
    String url,
    String outputPath,
    String fileName,
    bool forceOverwrite, {
    required bool showProgress,
  }) async {
    final client = _createHttpClient();

    try {
      if (showProgress) {
        print('\n' + '=' * 50);
        print('开始下载: $fileName');
        print('下载地址: $url');
        print('保存路径: $outputPath');
        print('=' * 50);
      } else {
        print('开始下载: $fileName');
        print('下载地址: $url');
        print('保存路径: $outputPath');
      }

      final file = File(outputPath);
      if (await file.exists()) {
        if (forceOverwrite) {
          print('[$fileName] 文件已存在，强制覆盖');
        } else {
          print('[$fileName] 文件已存在，跳过下载（使用 -f 强制覆盖）');
          return;
        }
      }

      final request = await client.getUrl(Uri.parse(url));
      final response = await request.close();

      if (response.statusCode != HttpStatus.ok) {
        throw HttpException('下载失败，状态码: ${response.statusCode}');
      }

      final total = response.contentLength;
      if (showProgress) {
        if (total == -1) {
          print('[$fileName] 无法获取文件大小，开始下载...');
        } else {
          print(
            '[$fileName] 文件大小: ${(total / 1024 / 1024).toStringAsFixed(2)} MB',
          );
        }
      }

      await file.parent.create(recursive: true);

      final sink = file.openWrite();

      if (showProgress && total != -1 && total > 0) {
        await _downloadWithProgress(response, sink, fileName, total);
      } else {
        await response.pipe(sink);
        await sink.close();
        print('[$fileName] 下载完成');
      }

      await _verifyFileIntegrity(file, total, fileName);
    } catch (e) {
      print('\n[$fileName] 下载失败: $e');
      rethrow;
    } finally {
      client.close();
    }
  }

  Future<void> _downloadWithProgress(
    HttpClientResponse response,
    IOSink sink,
    String fileName,
    int total,
  ) async {
    int received = 0;
    final stopwatch = Stopwatch()..start();
    DateTime lastUpdate = DateTime.now();

    await for (var chunk in response) {
      sink.add(chunk);
      received += chunk.length;

      final now = DateTime.now();
      if (now.difference(lastUpdate).inMilliseconds >= 200 ||
          received == total) {
        final progress = (received / total * 100).toStringAsFixed(1);
        final speed = stopwatch.elapsedMilliseconds > 0
            ? received / stopwatch.elapsedMilliseconds * 1000
            : 0;
        final remaining = total - received;
        final eta = speed > 0 ? remaining / speed.toDouble() : 0.0;

        print(
          '\r[$fileName] ${_createProgressBar(received, total)} $progress% '
          '| ${_formatBytes(received)}/${_formatBytes(total)} '
          '| ${_formatBytes(speed.toInt())}/s '
          '| ETA: ${_formatTime(eta)}     ',
        );
        lastUpdate = now;
      }
    }

    print(
      '\r[$fileName] ${_createProgressBar(total, total)} 100.0% '
      '| ${_formatBytes(total)}/${_formatBytes(total)} '
      '| 完成!                          ',
    );
    await sink.close();
    stopwatch.stop();

    print('\n[$fileName] 下载完成!');
    print(
      '[$fileName] 耗时: ${(stopwatch.elapsedMilliseconds / 1000).toStringAsFixed(1)}s',
    );
    final avgSpeed = stopwatch.elapsedMilliseconds > 0
        ? (received / stopwatch.elapsedMilliseconds * 1000).toInt()
        : received;
    print('[$fileName] 平均速度: ${_formatBytes(avgSpeed)}/s');
  }

  Future<void> _verifyFileIntegrity(
    File file,
    int expectedSize,
    String fileName,
  ) async {
    try {
      final actualSize = await file.length();

      if (expectedSize != -1 && actualSize != expectedSize) {
        print('[$fileName] 警告: 文件大小不匹配 (期望: $expectedSize, 实际: $actualSize)');
      } else {
        print('[$fileName] 文件完整性验证通过');
      }
    } catch (e) {
      print('[$fileName] 文件完整性验证失败: $e');
    }
  }

  Future<void> _downloadWithRetry(
    String url,
    String outputPath,
    String fileName,
    bool forceOverwrite,
    int maxRetries,
  ) async {
    for (int attempt = 1; attempt <= maxRetries; attempt++) {
      try {
        await downloadFileSimple(url, outputPath, fileName, forceOverwrite);
        return;
      } catch (e) {
        if (attempt == maxRetries) {
          rethrow;
        }
        print('[$fileName] 下载失败 (尝试 $attempt/$maxRetries): $e');
        print('[$fileName] 等待 ${attempt * 2} 秒后重试...');
        await Future.delayed(Duration(seconds: attempt * 2));
      }
    }
  }

  String _createProgressBar(int received, int total) {
    const width = 20;
    if (total <= 0) return '[' + '?' * width + ']';
    final progress = received / total;
    final filled = (progress * width).round();
    final empty = width - filled;
    return '[' + '=' * filled + ' ' * empty + ']';
  }

  String _formatBytes(int bytes) {
    if (bytes < 1024) return '$bytes B';
    if (bytes < 1024 * 1024) return '${(bytes / 1024).toStringAsFixed(1)} KB';
    if (bytes < 1024 * 1024 * 1024)
      return '${(bytes / 1024 / 1024).toStringAsFixed(1)} MB';
    return '${(bytes / 1024 / 1024 / 1024).toStringAsFixed(1)} GB';
  }

  String _formatTime(double seconds) {
    if (seconds.isInfinite || seconds.isNaN || seconds <= 0) return '--:--';
    final mins = (seconds / 60).floor();
    final secs = (seconds % 60).floor();
    return '${mins.toString().padLeft(2, '0')}:${secs.toString().padLeft(2, '0')}';
  }
}

class _DownloadProgress {
  final int total;
  int _completed = 0;
  int _success = 0;
  int _failures = 0;

  _DownloadProgress(this.total);

  int get completed => _completed;
  int get success => _success;
  int get failures => _failures;

  void incrementCompleted() {
    _completed++;
  }

  void incrementSuccess() {
    _success++;
  }

  void incrementFailure() {
    _failures++;
  }
}
