import 'dart:convert';

import 'package:flutter/foundation.dart';
import 'package:http/http.dart' as http;

class ApiException implements Exception {
  ApiException(this.message);
  final String message;
  @override
  String toString() => message;
}

class BondApi {
  BondApi({String? baseUrl}) : baseUrl = baseUrl ?? defaultBaseUrl();

  String baseUrl;
  String? token;

  static String defaultBaseUrl() {
    const baked = String.fromEnvironment('API_BASE');
    if (baked.isNotEmpty) return baked;
    if (kIsWeb) return Uri.base.origin;
    return 'http://192.168.1.63:11111';
  }

  Map<String, String> get _headers => {
        'Content-Type': 'application/json',
        if (token != null) 'Authorization': 'Bearer $token',
      };

  Future<Map<String, dynamic>> _send(
    String method,
    String path, {
    Map<String, dynamic>? body,
    Duration timeout = const Duration(seconds: 6),
  }) async {
    final uri = Uri.parse('$baseUrl$path');
    late http.Response res;
    try {
      switch (method) {
        case 'GET':
          res = await http.get(uri, headers: _headers).timeout(timeout);
        case 'PUT':
          res = await http.put(uri, headers: _headers, body: jsonEncode(body ?? {})).timeout(timeout);
        default:
          res = await http.post(uri, headers: _headers, body: jsonEncode(body ?? {})).timeout(timeout);
      }
    } catch (error) {
      throw ApiException('云端连不上：$error');
    }
    final decoded = jsonDecode(res.body.isEmpty ? '{}' : res.body);
    if (res.statusCode >= 400) {
      throw ApiException(decoded['detail']?.toString() ?? 'HTTP ${res.statusCode}');
    }
    return decoded as Map<String, dynamic>;
  }

  Future<bool> health() async {
    try {
      final data = await _send('GET', '/health');
      return data['ok'] == 'bondwatch';
    } catch (_) {
      return false;
    }
  }

  String? pairCode;
  String? displayName;

  Future<Map<String, dynamic>> login({
    required String username,
    required String password,
  }) async {
    final data = await _send('POST', '/v1/auth/login', body: {
      'username': username,
      'password': password,
    });
    token = data['token'] as String?;
    pairCode = data['pair_code'] as String?;
    displayName = data['display_name'] as String?;
    return data;
  }

  Future<Map<String, dynamic>> register({
    required String username,
    required String password,
    String? displayName,
  }) async {
    final data = await _send('POST', '/v1/auth/register', body: {
      'username': username,
      'password': password,
      if (displayName != null && displayName.isNotEmpty) 'display_name': displayName,
    });
    token = data['token'] as String?;
    pairCode = data['pair_code'] as String?;
    this.displayName = data['display_name'] as String?;
    return data;
  }

  Future<Map<String, dynamic>> updates() => _send('GET', '/v1/updates');

  Future<Map<String, dynamic>> me() => _send('GET', '/v1/me');

  Future<void> bind({String deviceId = 'watch-sim-1', String? code, String name = '假手表'}) {
    return _send('POST', '/v1/devices/bind', body: {
      'device_id': deviceId,
      'code': code ?? pairCode ?? '',
      'name': name,
    }).then((_) {});
  }

  Future<String> openSession({String holder = 'phone'}) async {
    final data = await _send('POST', '/v1/sessions', body: {
      'holder': holder,
      'device_id': 'watch-sim-1',
    });
    return data['session_id'] as String;
  }

  Future<Map<String, dynamic>> turn(String sessionId, String text, {String? imageBase64}) {
    return _send(
      'POST',
      '/v1/sessions/$sessionId/turn',
      body: {
        'text': text,
        if (imageBase64 != null && imageBase64.isNotEmpty) 'image_base64': imageBase64,
      },
      timeout: Duration(seconds: imageBase64 == null ? 6 : 20),
    );
  }

  Future<Map<String, dynamic>> memory() => _send('GET', '/v1/memory');

  Future<Map<String, dynamic>> saveMemory(String persona, String notes) {
    return _send('PUT', '/v1/memory', body: {'persona': persona, 'notes': notes});
  }

  Future<Map<String, dynamic>> alarms() => _send('GET', '/v1/alarms');

  Future<Map<String, dynamic>> addAlarm({int minutes = 1}) {
    return _send('PUT', '/v1/alarms', body: {
      'minutes': minutes,
      'label': '本地闹钟',
      'cached_line': '到点了。',
    });
  }

  Future<Map<String, dynamic>> settings() => _send('GET', '/v1/settings');

  Future<Map<String, dynamic>> saveSettings(Map<String, dynamic> body) {
    return _send('PUT', '/v1/settings', body: body);
  }

  Future<List<dynamic>> history() async {
    final data = await _send('GET', '/v1/history');
    return data['items'] as List<dynamic>? ?? [];
  }
}
