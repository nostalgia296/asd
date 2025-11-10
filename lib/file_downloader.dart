import 'dart:async';
import 'dart:io';
import 'package:pool/pool.dart';
import 'package:downloader/utils.dart';

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
    int successCount = 0;
    int completedCount = 0;

    final pool = Pool(5);

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
        successCount++;
      } catch (e) {
        final fileName = fileInfo['name']!;
        failures.add(DownloadFailure(fileName: fileName, error: e.toString()));
      } finally {
        completedCount++;
        if (files.length > 1) {
          print(
            '\r下载进度: $completedCount/${files.length} | 成功: $successCount | 失败: ${failures.length}',
          );
        }
        resource.release();
      }
    }).toList();

    await Future.wait(tasks);

    return DownloadResult(
      successCount: successCount,
      failureCount: failures.length,
      failures: failures,
    );
  }

  Future<void> downloadFileWithProgress(
    String url,
    String outputPath,
    String fileName,
    bool forceOverwrite,
  ) async {
    final client = HttpClient();

    try {
      print('\n' + '=' * 50);
      print('开始下载: $fileName');
      print('下载地址: $url');
      print('保存路径: $outputPath');
      print('=' * 50);

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
      if (total == -1) {
        print('[$fileName] 无法获取文件大小，开始下载...');
      } else {
        print(
          '[$fileName] 文件大小: ${(total / 1024 / 1024).toStringAsFixed(2)} MB',
        );
      }

      await file.parent.create(recursive: true);

      final sink = file.openWrite();

      int received = 0;
      final stopwatch = Stopwatch()..start();

      int initTime = 0;
      await for (var chunk in response) {
        sink.add(chunk);
        received += chunk.length;
        initTime++;

        if (total != -1 && total > 0 && initTime % 50 == 0) {
          final progress = (received / total * 100).toStringAsFixed(1);
          final speed = stopwatch.elapsedMilliseconds > 0
              ? received / stopwatch.elapsedMilliseconds * 1000
              : 0;
          final remaining = total - received;
          final eta = speed > 0 ? remaining / speed.toDouble() : 0.0;

          print(
            '\r[$fileName] ${createProgressBar(received, total)} $progress% '
            '| ${formatBytes(received)}/${formatBytes(total)} '
            '| ${formatBytes(speed.toInt())}/s '
            '| ETA: ${formatTime(eta)}     ',
          );
        }
      }
      if (total != -1 && total > 0) {
        print(
          '\r[$fileName] ${createProgressBar(total, total)} 100.0% '
          '| ${formatBytes(total)}/${formatBytes(total)} '
          '| 完成!                          ',
        );
      }
      await sink.close();
      stopwatch.stop();

      print('\n[$fileName] 下载完成!');
      print(
        '[$fileName] 耗时: ${(stopwatch.elapsedMilliseconds / 1000).toStringAsFixed(1)}s',
      );
      final avgSpeed = stopwatch.elapsedMilliseconds > 0
          ? (received / stopwatch.elapsedMilliseconds * 1000).toInt()
          : received;
      print('[$fileName] 平均速度: ${formatBytes(avgSpeed)}/s');
    } catch (e) {
      print('\n[$fileName] 下载失败: $e');
      rethrow;
    } finally {
      client.close();
    }
  }

  Future<void> downloadFileSimple(
    String url,
    String outputPath,
    String fileName,
    bool forceOverwrite,
  ) async {
    final client = HttpClient();

    try {
      print('开始下载: $fileName');
      print('下载地址: $url');
      print('保存路径: $outputPath');

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

      await file.parent.create(recursive: true);

      final sink = file.openWrite();
      await response.pipe(sink);
      await sink.close();

      print('[$fileName] 下载完成');
    } catch (e) {
      print('[$fileName] 下载失败: $e');
      rethrow;
    } finally {
      client.close();
    }
  }
}
