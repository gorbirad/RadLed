import 'dart:async';
import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;
import 'package:shared_preferences/shared_preferences.dart';

void main() {
  runApp(const RadLedApp());
}

class RadLedApp extends StatelessWidget {
  const RadLedApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'RadLed Controller',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: Colors.teal),
        useMaterial3: true,
      ),
      home: const ControllerPage(),
    );
  }
}

class RunnerModel {
  RunnerModel({
    required this.id,
    required this.tempo,
    required this.speed,
    required this.pos,
    required this.length,
    required this.hex,
    required this.name,
  });

  final int id;
  final double tempo;
  final double speed;
  final double pos;
  final int length;
  final String hex;
  final String name;

  factory RunnerModel.fromJson(Map<String, dynamic> json) {
    return RunnerModel(
      id: (json['id'] ?? 0) as int,
      tempo: ((json['tempo'] ?? 0) as num).toDouble(),
      speed: ((json['speed'] ?? 0) as num).toDouble(),
      pos: ((json['pos'] ?? 0) as num).toDouble(),
      length: (json['length'] ?? 0) as int,
      hex: (json['hex'] ?? '#000000') as String,
      name: (json['name'] ?? 'Runner') as String,
    );
  }
}

class ControllerPage extends StatefulWidget {
  const ControllerPage({super.key});

  @override
  State<ControllerPage> createState() => _ControllerPageState();
}

class _ControllerPageState extends State<ControllerPage> {
  static const String defaultHost = '192.168.4.1';

  final TextEditingController hostController = TextEditingController(
    text: defaultHost,
  );

  final TextEditingController tempoController = TextEditingController(text: '5');
  final TextEditingController lengthController = TextEditingController(text: '10');

  int red = 255;
  int green = 0;
  int blue = 0;
  int brightness = 120;

  bool loading = false;
  String statusLine = 'Disconnected';
  String wifiMode = '-';
  String errorText = '-';
  List<RunnerModel> runners = <RunnerModel>[];

  Timer? pollTimer;

  @override
  void initState() {
    super.initState();
    _loadHost();
    pollTimer = Timer.periodic(const Duration(seconds: 2), (_) {
      _refreshAll(silent: true);
    });
  }

  @override
  void dispose() {
    pollTimer?.cancel();
    hostController.dispose();
    tempoController.dispose();
    lengthController.dispose();
    super.dispose();
  }

  Future<void> _loadHost() async {
    final SharedPreferences prefs = await SharedPreferences.getInstance();
    final String host = prefs.getString('radled_host') ?? defaultHost;
    hostController.text = host;
    await _refreshAll();
  }

  Future<void> _saveHost() async {
    final SharedPreferences prefs = await SharedPreferences.getInstance();
    await prefs.setString('radled_host', hostController.text.trim());
  }

  Uri _uri(String path, [Map<String, String>? query]) {
    return Uri.http(hostController.text.trim(), path, query);
  }

  Future<dynamic> _getJson(String path, [Map<String, String>? query]) async {
    final http.Response response = await http
        .get(_uri(path, query))
        .timeout(const Duration(seconds: 4));
    if (response.statusCode >= 400) {
      throw Exception('HTTP ${response.statusCode}: ${response.body}');
    }
    return jsonDecode(response.body);
  }

  Future<String> _getText(String path, [Map<String, String>? query]) async {
    final http.Response response = await http
        .get(_uri(path, query))
        .timeout(const Duration(seconds: 4));
    if (response.statusCode >= 400) {
      throw Exception('HTTP ${response.statusCode}: ${response.body}');
    }
    return response.body;
  }

  Future<void> _refreshAll({bool silent = false}) async {
    if (!silent) {
      setState(() {
        loading = true;
      });
    }

    try {
      final Map<String, dynamic> status =
          (await _getJson('/status')) as Map<String, dynamic>;
      final Map<String, dynamic> wifi =
          (await _getJson('/wifiInfo')) as Map<String, dynamic>;
      final List<dynamic> list = (await _getJson('/listRunners')) as List<dynamic>;

      setState(() {
        brightness = ((status['brightness'] ?? brightness) as num).toInt();
        wifiMode = (wifi['mode'] ?? '-') as String;
        errorText = (status['error'] ?? '-') as String;
        statusLine =
            'running=${status['animationRunning']} reverse=${status['reverseDirection']}';
        runners = list
            .map((dynamic e) => RunnerModel.fromJson(e as Map<String, dynamic>))
            .toList();
      });
    } catch (e) {
      if (!silent) {
        setState(() {
          statusLine = 'Connection error';
          errorText = e.toString();
        });
      }
    } finally {
      if (!silent && mounted) {
        setState(() {
          loading = false;
        });
      }
    }
  }

  Future<void> _command(String path, {Map<String, String>? query}) async {
    setState(() {
      loading = true;
    });
    try {
      await _getText(path, query);
      await _refreshAll(silent: true);
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('OK: $path')),
      );
    } catch (e) {
      if (!mounted) return;
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Blad: $e')),
      );
    } finally {
      if (mounted) {
        setState(() {
          loading = false;
        });
      }
    }
  }

  Future<void> _addRunner() async {
    final String tempo = tempoController.text.trim();
    final String len = lengthController.text.trim();
    await _command('/addRunner', query: <String, String>{
      'tempo': tempo,
      'len': len,
      'r': red.toString(),
      'g': green.toString(),
      'b': blue.toString(),
    });
  }

  Color _previewColor() => Color.fromARGB(255, red, green, blue);

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('RadLed Controller'),
        actions: <Widget>[
          IconButton(
            icon: const Icon(Icons.refresh),
            onPressed: loading ? null : _refreshAll,
          ),
        ],
      ),
      body: SafeArea(
        child: ListView(
          padding: const EdgeInsets.all(16),
          children: <Widget>[
            Card(
              child: Padding(
                padding: const EdgeInsets.all(12),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: <Widget>[
                    const Text('Polaczenie', style: TextStyle(fontWeight: FontWeight.bold)),
                    const SizedBox(height: 8),
                    TextField(
                      controller: hostController,
                      decoration: const InputDecoration(
                        labelText: 'Host/IP (np. 192.168.4.1)',
                        border: OutlineInputBorder(),
                      ),
                    ),
                    const SizedBox(height: 8),
                    Wrap(
                      spacing: 8,
                      runSpacing: 8,
                      children: <Widget>[
                        FilledButton(
                          onPressed: loading
                              ? null
                              : () async {
                                  await _saveHost();
                                  await _refreshAll();
                                },
                          child: const Text('Zapisz i testuj'),
                        ),
                        OutlinedButton(
                          onPressed: loading
                              ? null
                              : () {
                                  hostController.text = defaultHost;
                                },
                          child: const Text('AP default 192.168.4.1'),
                        ),
                      ],
                    ),
                    const SizedBox(height: 8),
                    Text('status: $statusLine'),
                    Text('wifiMode: $wifiMode'),
                    Text('error: $errorText'),
                  ],
                ),
              ),
            ),
            Card(
              child: Padding(
                padding: const EdgeInsets.all(12),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: <Widget>[
                    const Text('Sterowanie', style: TextStyle(fontWeight: FontWeight.bold)),
                    const SizedBox(height: 8),
                    Wrap(
                      spacing: 8,
                      runSpacing: 8,
                      children: <Widget>[
                        FilledButton(
                          onPressed: loading ? null : () => _command('/start'),
                          child: const Text('Start'),
                        ),
                        FilledButton.tonal(
                          onPressed: loading ? null : () => _command('/stopAll'),
                          child: const Text('Stop All'),
                        ),
                        OutlinedButton(
                          onPressed: loading ? null : () => _command('/reverse'),
                          child: const Text('Reverse'),
                        ),
                        OutlinedButton(
                          onPressed: loading ? null : () => _command('/clear'),
                          child: const Text('Clear'),
                        ),
                      ],
                    ),
                    const SizedBox(height: 12),
                    Text('Jasnosc: $brightness'),
                    Slider(
                      min: 0,
                      max: 255,
                      divisions: 255,
                      value: brightness.toDouble(),
                      label: '$brightness',
                      onChanged: (double value) {
                        setState(() {
                          brightness = value.round();
                        });
                      },
                      onChangeEnd: (double value) {
                        _command('/brightness', query: <String, String>{
                          'value': value.round().toString(),
                        });
                      },
                    ),
                  ],
                ),
              ),
            ),
            Card(
              child: Padding(
                padding: const EdgeInsets.all(12),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: <Widget>[
                    const Text('Dodaj biegacza', style: TextStyle(fontWeight: FontWeight.bold)),
                    const SizedBox(height: 8),
                    Row(
                      children: <Widget>[
                        Expanded(
                          child: TextField(
                            controller: tempoController,
                            keyboardType: const TextInputType.numberWithOptions(decimal: true),
                            decoration: const InputDecoration(
                              labelText: 'Tempo (min/km)',
                              border: OutlineInputBorder(),
                            ),
                          ),
                        ),
                        const SizedBox(width: 8),
                        Expanded(
                          child: TextField(
                            controller: lengthController,
                            keyboardType: TextInputType.number,
                            decoration: const InputDecoration(
                              labelText: 'Dl. ogona',
                              border: OutlineInputBorder(),
                            ),
                          ),
                        ),
                      ],
                    ),
                    const SizedBox(height: 8),
                    Container(
                      height: 24,
                      decoration: BoxDecoration(
                        color: _previewColor(),
                        borderRadius: BorderRadius.circular(6),
                        border: Border.all(color: Colors.black26),
                      ),
                    ),
                    const SizedBox(height: 8),
                    Text('R: $red'),
                    Slider(
                      min: 0,
                      max: 255,
                      divisions: 255,
                      value: red.toDouble(),
                      onChanged: (double value) => setState(() => red = value.round()),
                    ),
                    Text('G: $green'),
                    Slider(
                      min: 0,
                      max: 255,
                      divisions: 255,
                      value: green.toDouble(),
                      onChanged: (double value) => setState(() => green = value.round()),
                    ),
                    Text('B: $blue'),
                    Slider(
                      min: 0,
                      max: 255,
                      divisions: 255,
                      value: blue.toDouble(),
                      onChanged: (double value) => setState(() => blue = value.round()),
                    ),
                    FilledButton(
                      onPressed: loading ? null : _addRunner,
                      child: const Text('Dodaj Runnera'),
                    ),
                  ],
                ),
              ),
            ),
            Card(
              child: Padding(
                padding: const EdgeInsets.all(12),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: <Widget>[
                    const Text('Aktywni biegacze', style: TextStyle(fontWeight: FontWeight.bold)),
                    const SizedBox(height: 8),
                    if (runners.isEmpty)
                      const Text('Brak aktywnych runnerow')
                    else
                      ...runners.map((RunnerModel runner) {
                        return ListTile(
                          contentPadding: EdgeInsets.zero,
                          leading: CircleAvatar(backgroundColor: _hexToColor(runner.hex)),
                          title: Text('${runner.name} (ID ${runner.id})'),
                          subtitle: Text(
                            'tempo ${runner.tempo.toStringAsFixed(2)} | '
                            'pos ${runner.pos.toStringAsFixed(1)} | '
                            'len ${runner.length}',
                          ),
                          trailing: IconButton(
                            icon: const Icon(Icons.stop_circle_outlined),
                            onPressed: loading
                                ? null
                                : () => _command(
                                      '/stopRunner',
                                      query: <String, String>{'id': runner.id.toString()},
                                    ),
                          ),
                        );
                      }),
                  ],
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Color _hexToColor(String hex) {
    final String raw = hex.replaceAll('#', '');
    if (raw.length != 6) {
      return Colors.black;
    }
    final int value = int.tryParse(raw, radix: 16) ?? 0;
    return Color(0xFF000000 | value);
  }
}
