import os
import json
import pandas as pd
import dash_bootstrap_components as dbc
import dash
from dash import dcc, html, dash_table, Input, Output, State, ctx, MATCH, ALL, callback_context
import plotly.express as px
import plotly.graph_objs as go
from plotly.subplots import make_subplots
import datetime
import numpy as np
from dash.exceptions import PreventUpdate
import re
from collections import defaultdict
import base64 # For dcc.Upload
import io # For dcc.Upload

# --- File Paths ---
# Default path to the JSON results file
DEFAULT_RESULTS_PATH = os.path.join(os.path.dirname(__file__) if '__file__' in locals() else '.', 'output_benchmarks/performance_results_full_data.json')

# --- Styling Constants ---
PRIMARY_COLOR = '#1f77b4' 
ACCENT_COLOR = '#ff7f0e'  
NEUTRAL_COLOR = '#505f79' 
MAIN_BG_COLOR = '#f0f2f5' 
CARD_BG_COLOR = '#ffffff'
SUCCESS_COLOR = '#28a745' 
WARNING_COLOR = '#ffc107' 
ERROR_COLOR = '#dc3545'   
PLOT_BG_COLOR = 'rgba(229, 236, 246, 0.3)' 
GRID_COLOR = '#d1d9e6'
MARKER_OUTLINE_COLOR = 'rgba(0,0,0,0.7)'
TEXT_MUTED_COLOR = '#6c757d' 

# --- PlotHelpTextManager Class ---
class PlotHelpTextManager:
    """Manages help text descriptions for plots."""
    def __init__(self):
        self.help_texts = {
            "scaling": """
**Execution Time Scaling**

This plot shows the mean execution time (log scale) against the number of threads for different examples and problem sizes.
- **CARTS** version is shown with a solid blue line.
- **OMP** version is shown with a dashed orange line.
- Each line represents a unique combination of example, version, and problem size based on the aggregated average times.
            """,
            "efficiency": """
**Parallel Efficiency (Strong Scaling)**

This plot illustrates how well the parallel versions (CARTS and OMP) utilize additional threads compared to their single-threaded performance.
- Efficiency is calculated as: `Average Time(1 thread) / (Average Time(N threads) * N threads)`.
- An ideal efficiency is 1.0 (dashed grey line).
- Values closer to 1.0 indicate better scalability.
            """,
            "speedup": """
**OMP vs CARTS Speedup (OMP Avg Time / CARTS Avg Time)**

This plot shows the speedup of CARTS relative to OMP, using their respective average execution times.
- Calculated as: `Mean OMP Time / Mean CARTS Time` for the same example, problem size, and thread count.
- Values > 1 (green shaded area) indicate CARTS is faster.
- Values < 1 (red shaded area) indicate OMP is faster.
- A value of 1 (dashed grey line) means equal performance.
            """,
            "slowdown": """
**CARTS vs OMP Slowdown (CARTS Avg Time / OMP Avg Time)**

This plot shows the slowdown of CARTS relative to OMP (inverse of the speedup plot).
- Calculated as: `Mean CARTS Time / Mean OMP Time`.
- Values > 1 (red shaded area) indicate CARTS is slower.
- Values < 1 (green shaded area) indicate CARTS is faster.
- A value of 1 (dashed grey line) means equal performance.
            """,
            "variability_base": """
**Execution Time Variability**

These box plots show the distribution of individual execution times (log scale) from raw benchmark runs for each combination of example, problem size, version, and thread count.
- Each box represents the interquartile range (IQR), with the median as a line.
- Whiskers extend to 1.5x IQR. Points beyond are outliers.
- Useful for observing consistency of runtimes across multiple iterations.
            """,
            "cache_perf": """
**Cache Performance Analysis**

- If 'Cache Miss Rate' is available (derived from `cache-misses` / `cache-references`), it's shown as a percentage.
- Otherwise, raw counts for a primary cache event (e.g., `cache-misses:u`) are plotted on a log scale.
            """,
            "ipc_perf": """
**Instructions Per Cycle (IPC) Analysis**

- Shows IPC, derived from average `instructions:u` / average `cycles:u`.
- Higher IPC generally indicates better CPU utilization.
            """,
            "stalls_perf": """
**CPU Stalls Analysis**

- Shows data for a primary stall counter (e.g., `cycle_activity.stalls_total:u`).
- May be displayed as raw counts (log scale) or as a percentage of total cycles if applicable and `cycles:u` data is available.
            """,
            "generic_profiling": """
**Generic Profiling Event Analysis**

- Shows the average value for the most prominent event within the selected 'Other' category.
- The y-axis may be linear or log scale depending on the data range.
            """
        }

    def get_help_text(self, plot_key, specific_event_info=""):
        base_text = self.help_texts.get(plot_key, "Help text not available for this plot.")
        if specific_event_info:
            separator = "\n" if base_text and not base_text.endswith("\n") else ""
            return base_text + separator + specific_event_info
        return base_text

# --- PlotTemplate class ---
class PlotTemplate:
    def __init__(self,
                 legend_orientation='h', legend_y=-0.25, legend_x=0.5,
                 height=600, margin=None, bargap=0.18,
                 grid_color=GRID_COLOR, axis_color='#666666', plot_bgcolor=PLOT_BG_COLOR, paper_bgcolor='white',
                 bar_line_width=1.5, bar_width=0.4, line_width=2, marker_line_width=1,
                 font_family='"Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif', font_size=14,
                 xaxis_title='', yaxis_title='', plot_title='', legend_title=''):
        if margin is None:
            margin = dict(l=60, r=40, t=80, b=120)
        self.legend_orientation = legend_orientation
        self.legend_y = legend_y
        self.legend_x = legend_x
        self.height = height
        self.margin = margin
        self.bargap = bargap
        self.grid_color = grid_color
        self.axis_color = axis_color
        self.plot_bgcolor = plot_bgcolor
        self.paper_bgcolor = paper_bgcolor
        self.bar_line_width = bar_line_width
        self.bar_width = bar_width
        self.line_width = line_width
        self.marker_line_width = marker_line_width
        self.font_family = font_family
        self.font_size = font_size
        self.xaxis_title_size = font_size + 1
        self.plot_title_size = font_size + 4
        self.xaxis_title = xaxis_title
        self.yaxis_title = yaxis_title
        self.plot_title = plot_title
        self.legend_title = legend_title

    def apply(self, fig, xaxis_title=None, yaxis_title=None, plot_title=None, legend_title=None, height=None):
        fig_height = height if height is not None else self.height
        current_margin = self.margin.copy()
        title_text_val = plot_title if plot_title is not None else self.plot_title
        legend_title_val = legend_title if legend_title is not None else self.legend_title

        if self.legend_orientation == 'h' and self.legend_y < 0:
            current_margin['b'] = max(current_margin.get('b', 120), 120 + abs(self.legend_y * 200))
        else:
            current_margin['b'] = max(60, current_margin.get('b', 100) - 40)

        if not title_text_val:
            current_margin['t'] = max(30, current_margin.get('t', 80) - 40)

        fig.update_layout(
            height=fig_height,
            title=dict(text=title_text_val, x=0.5, font=dict(size=self.plot_title_size, family=self.font_family)),
            font=dict(family=self.font_family, size=self.font_size),
            legend=dict(
                title=dict(text=legend_title_val, font=dict(size=self.font_size -1)),
                orientation=self.legend_orientation, yanchor='top', y=self.legend_y,
                xanchor='center', x=self.legend_x, bgcolor='rgba(255,255,255,0.85)',
                bordercolor='#cccccc', borderwidth=1, font=dict(size=self.font_size - 2)
            ),
            plot_bgcolor=self.plot_bgcolor, paper_bgcolor=self.paper_bgcolor, margin=current_margin, bargap=self.bargap,
            xaxis=dict(
                title=dict(text=xaxis_title if xaxis_title is not None else self.xaxis_title, font=dict(size=self.xaxis_title_size, color=NEUTRAL_COLOR)),
                tickfont=dict(size=self.font_size - 2), gridcolor=self.grid_color, showgrid=True,
                showline=True, linecolor=self.axis_color, ticks='outside', automargin=True,
            ),
            yaxis=dict(
                title=dict(text=yaxis_title if yaxis_title is not None else self.yaxis_title, font=dict(size=self.xaxis_title_size, color=NEUTRAL_COLOR)),
                tickfont=dict(size=self.font_size - 2), gridcolor=self.grid_color, showgrid=True,
                showline=True, linecolor=self.axis_color, ticks='outside', automargin=True,
            ),
            hoverlabel=dict(bgcolor="white", font_size=self.font_size -1, font_family=self.font_family, bordercolor="#cccccc", font_color="black")
        )
        if fig.layout.annotations:
            fig.update_annotations(font_size=self.font_size)

        for trace in fig.data:
            if trace.type == 'bar':
                if not hasattr(trace.marker, 'color') or trace.marker.color is None:
                    trace.marker.color = PRIMARY_COLOR
                if not hasattr(trace.marker, 'line') or not hasattr(trace.marker.line, 'width') or trace.marker.line.width is None:
                    trace.marker.line.width = self.bar_line_width
                if not hasattr(trace.marker.line, 'color') or trace.marker.line.color is None:
                    trace.marker.line.color = trace.marker.color if trace.marker.color else MARKER_OUTLINE_COLOR 
            elif trace.type == 'scatter':
                if hasattr(trace, 'line') and (not hasattr(trace.line, 'width') or trace.line.width is None):
                    trace.line.width = self.line_width
                if hasattr(trace, 'mode') and 'markers' in trace.mode:
                    if not hasattr(trace.marker, 'size') or trace.marker.size is None:
                        trace.marker.size = 8
                    if not hasattr(trace.marker, 'line') or trace.marker.line is None:
                        trace.marker.line = go.scatter.marker.Line()
                    if not hasattr(trace.marker.line, 'width') or trace.marker.line.width is None:
                        trace.marker.line.width = self.marker_line_width
                    if not hasattr(trace.marker.line, 'color') or trace.marker.line.color is None:
                        trace.marker.line.color = MARKER_OUTLINE_COLOR
        return fig

default_plot_template = PlotTemplate(
    yaxis_title='Value',
    legend_title='Version'
)
plot_help_manager = PlotHelpTextManager()

# --- StyleManager Class ---
class StyleManager: 
    MAIN_DIV_STYLE = {
        'background': MAIN_BG_COLOR, 'minHeight': '100vh',
        'fontFamily': default_plot_template.font_family, 'color': NEUTRAL_COLOR,
    }
    CARD_STYLE = {
        'backgroundColor': CARD_BG_COLOR, 'borderRadius': '12px', 'boxShadow': '0 6px 18px rgba(0,0,0,0.07)',
        'padding': '24px', 'margin': '20px auto', 'maxWidth': '1400px', 'border': f'1px solid {GRID_COLOR}',
    }
    HEADER_STYLE = {
        'color': PRIMARY_COLOR, 'fontWeight': '600', 'fontSize': '2.2rem', 'marginBottom': '8px',
        'textAlign': 'center', 'letterSpacing': '0.3px', 'padding': '20px 0 10px 0',
        'borderBottom': f'2px solid {PRIMARY_COLOR}', 'background': 'transparent',
    }
    SUBTITLE_STYLE = {
        'color': TEXT_MUTED_COLOR, 'fontSize': '1.1rem', 'textAlign': 'center', 'marginBottom': '24px',
        'fontWeight': '400', 'maxWidth': '900px', 'margin': '0 auto 24px auto',
    }
    LABEL_STYLE = {
        'fontWeight': '600', 'color': PRIMARY_COLOR, 'marginTop': '14px',
        'fontSize': '1.05rem', 'letterSpacing': '0.2px',
    }
    FILTER_MODAL_CLOSE_STYLE = { 
        'fontSize': '1.5rem', 'fontWeight': 'bold', 'lineHeight': '1',
        'color': '#000', 'textShadow': '0 1px 0 #fff', 'opacity': '0.5',
        'position': 'absolute', 'top': '10px', 'right': '15px',
        'background': 'transparent', 'border': '0'
    }
    STAT_BOX_STYLE = {
        'backgroundColor': CARD_BG_COLOR, 
        'padding': '15px',
        'borderRadius': '8px',
        'boxShadow': '0 3px 8px rgba(0,0,0,0.06)',
        'textAlign': 'center',
        'marginBottom': '15px', 
        'border': f'1px solid {GRID_COLOR}'
    }
    STAT_BOX_LABEL_STYLE = {'fontSize': '0.9rem', 'color': TEXT_MUTED_COLOR, 'marginBottom': '5px', 'display': 'block', 'fontWeight': '500'}
    STAT_BOX_VALUE_STYLE = {'fontSize': '1.6rem', 'fontWeight': 'bold', 'color': PRIMARY_COLOR, 'display': 'block', 'lineHeight': '1.2'}
    STAT_BOX_SUB_VALUE_STYLE = {'fontSize': '0.8rem', 'color': NEUTRAL_COLOR, 'marginTop': '4px', 'display': 'block', 'wordWrap': 'break-word', 'maxWidth': '100%', 'minHeight': '30px'}

    TABLE_HEADER_STYLE = {
        'backgroundColor': PRIMARY_COLOR, 'fontWeight': 'bold', 'fontSize': '1.05rem',
        'color': 'white', 'borderBottom': f'2px solid {PRIMARY_COLOR}',
        'fontFamily': default_plot_template.font_family, 'padding': '10px 8px'
    }
    TABLE_CELL_STYLE = {
        'fontSize': '1rem', 'padding': '8px', 'minWidth': '70px',
        'fontFamily': default_plot_template.font_family, 'border': f'1px solid {GRID_COLOR}'
    }
    UPLOAD_STYLE = {
        'width': '100%', 'height': '60px', 'lineHeight': '60px',
        'borderWidth': '2px', 'borderStyle': 'dashed', 'borderRadius': '8px',
        'textAlign': 'center', 'margin': '0px', 'borderColor': PRIMARY_COLOR, # No top/bottom margin for tighter layout
        'cursor': 'pointer', 'transition': 'border-color .15s ease-in-out, box-shadow .15s ease-in-out',
        'fontSize': '0.95rem', 'color': PRIMARY_COLOR
    }
    UPLOAD_CONTAINER_STYLE = {
        'padding': '10px',
        'border': f'1px solid {GRID_COLOR}',
        'borderRadius': '8px',
        'backgroundColor': '#f8f9fa'
    }

# --- Utility Functions ---
def load_full_json_data(path):
    if not os.path.exists(path):
        return None, f"Results file not found: {path}"
    try:
        with open(path, 'r') as f:
            data = json.load(f)
        required_top_level_keys = [
            "detailed_aggregated_results", "raw_benchmark_runs", "raw_profiling_runs",
            "overall_statistics", "system_information"
        ]
        for key in required_top_level_keys:
            if key not in data:
                print(f"Warning: Key '{key}' missing in JSON data from {path}.")
        return data, None
    except json.JSONDecodeError as e:
        return None, f"Error decoding JSON from {path}: {e}"
    except Exception as e:
        return None, f"Error loading file {path}: {e}"

def format_number(val, ndigits=4, na_val="-"):
    if val is None or (isinstance(val, float) and (np.isnan(val) or np.isinf(val))): return na_val
    if isinstance(val, (int, np.integer)): return f"{val:,}"
    try:
        float_val = float(val)
        if ndigits == 0: 
             return f"{float_val:,.0f}"
        return f"{float_val:,.{ndigits}f}"
    except (ValueError, TypeError): return str(val)

# --- DataManager Class ---
class DataManager:
    def __init__(self, full_json_data=None):
        self.df_aggregated = pd.DataFrame()
        self.df_raw_benchmark = pd.DataFrame()
        self.df_raw_profiling = pd.DataFrame()
        self.system_info = {}
        self.overall_stats_json = {}
        self.reproducibility_info = {}
        self.command_line_args = {}
        self.failure_summary = []

        if full_json_data and isinstance(full_json_data, dict):
            self.df_aggregated = pd.DataFrame(full_json_data.get("detailed_aggregated_results", []))
            self.df_raw_benchmark = pd.DataFrame(full_json_data.get("raw_benchmark_runs", []))
            self.df_raw_profiling = pd.DataFrame(full_json_data.get("raw_profiling_runs", []))
            
            self.system_info = full_json_data.get("system_information", {})
            self.overall_stats_json = full_json_data.get("overall_statistics", {})
            self.reproducibility_info = full_json_data.get("reproducibility_info", {})
            self.command_line_args = full_json_data.get("command_line_args", {})
            self.failure_summary = full_json_data.get("failure_summary", [])

            self._convert_df_types()

    def _convert_df_types(self):
        if not self.df_aggregated.empty:
            if 'Threads' in self.df_aggregated.columns:
                self.df_aggregated['Threads'] = pd.to_numeric(self.df_aggregated['Threads'], errors='coerce')
            if 'Problem Size' in self.df_aggregated.columns:
                 self.df_aggregated['Problem Size'] = self.df_aggregated['Problem Size'].astype(str)
            for col in self.df_aggregated.columns:
                if 'Time (s)' in col or 'Speedup' in col or 'Event' in col: 
                    if 'Correctness' not in col and 'Config' not in col and 'Name' not in col and 'Size' not in col: 
                         self.df_aggregated[col] = pd.to_numeric(self.df_aggregated[col], errors='coerce')
        
        for df_raw in [self.df_raw_benchmark, self.df_raw_profiling]:
            if not df_raw.empty:
                if 'threads' in df_raw.columns:
                    df_raw['threads'] = pd.to_numeric(df_raw['threads'], errors='coerce')
                if 'problem_size' in df_raw.columns:
                    df_raw['problem_size'] = df_raw['problem_size'].astype(str)
                if 'times_seconds' in df_raw.columns: 
                    def process_times(times_entry):
                        if isinstance(times_entry, list):
                            return [float(t) for t in times_entry if isinstance(t, (int, float, str)) and str(t).replace('.', '', 1).isdigit()]
                        if isinstance(times_entry, str):
                            try: return [float(t.strip()) for t in times_entry.split(',') if t.strip().replace('.', '', 1).isdigit()]
                            except ValueError: return []
                        return []
                    df_raw['times_seconds'] = df_raw['times_seconds'].apply(process_times)

    def _apply_filters(self, df, filters, is_aggregated_df=False):
        dff = df.copy()
        if not filters or df.empty: return dff
        
        ex_col = 'Example Name' if is_aggregated_df else 'example_name'
        th_col = 'Threads' if is_aggregated_df else 'threads'
        ps_col = 'Problem Size' if is_aggregated_df else 'problem_size'
        
        example_filter_values = filters.get('examples')
        if example_filter_values and ex_col in dff.columns:
            dff = dff[dff[ex_col].isin(example_filter_values)]

        thread_filter_values = filters.get('threads')
        if thread_filter_values and th_col in dff.columns:
            filter_threads_numeric = [pd.to_numeric(t, errors='coerce') for t in thread_filter_values]
            filter_threads_numeric = [t for t in filter_threads_numeric if pd.notna(t)]
            if filter_threads_numeric: dff = dff[dff[th_col].isin(filter_threads_numeric)]

        size_filter_values = filters.get('sizes')
        if size_filter_values and ps_col in dff.columns:
            dff = dff[dff[ps_col].isin([str(s) for s in size_filter_values])]
        
        correctness_filter = filters.get('correctness')
        if correctness_filter and correctness_filter != 'ALL':
            if is_aggregated_df:
                if 'CARTS Correctness' in dff.columns and 'OMP Correctness' in dff.columns:
                     dff = dff[
                        (dff['CARTS Correctness'].astype(str).str.upper() == correctness_filter) |
                        (dff['OMP Correctness'].astype(str).str.upper() == correctness_filter)
                    ]
            elif 'correctness_status' in dff.columns: 
                dff = dff[dff['correctness_status'].astype(str).str.upper() == correctness_filter]
        return dff

    def get_filtered_aggregated_data(self, filters=None):
        if self.df_aggregated.empty: return pd.DataFrame()
        return self._apply_filters(self.df_aggregated, filters, is_aggregated_df=True)

    def get_filtered_raw_benchmark_data(self, filters=None):
        if self.df_raw_benchmark.empty: return pd.DataFrame()
        return self._apply_filters(self.df_raw_benchmark, filters, is_aggregated_df=False)
        
    def get_benchmark_display_data(self, filters=None):
        filtered_agg_df = self.get_filtered_aggregated_data(filters)
        if filtered_agg_df.empty: return pd.DataFrame()
        
        t1_avg_times_map = {}
        if not self.df_aggregated.empty:
            t1_df = self.df_aggregated[self.df_aggregated['Threads'] == 1].copy()
            for tc in ['CARTS Avg Time (s)', 'OMP Avg Time (s)']:
                if tc in t1_df.columns:
                    t1_df[tc] = pd.to_numeric(t1_df[tc], errors='coerce')

            for _, row in t1_df.iterrows():
                key = (row['Example Name'], row['Problem Size'])
                t1_avg_times_map[key] = {
                    'CARTS': row.get('CARTS Avg Time (s)'),
                    'OMP': row.get('OMP Avg Time (s)')
                }
        
        results_with_efficiency = []
        for _, row_series in filtered_agg_df.iterrows():
            row = row_series.to_dict()
            key = (row['Example Name'], row['Problem Size'])
            threads_N = row['Threads']

            for version_prefix in ["CARTS", "OMP"]:
                t1_avg_time_specific = t1_avg_times_map.get(key, {}).get(version_prefix)
                tn_avg_time_specific_val = row.get(f'{version_prefix} Avg Time (s)')
                tn_avg_time_specific = pd.to_numeric(tn_avg_time_specific_val, errors='coerce')
                
                efficiency = None
                if pd.notna(threads_N) and threads_N > 1 and pd.notna(t1_avg_time_specific) and pd.notna(tn_avg_time_specific) and tn_avg_time_specific > 0:
                    efficiency = t1_avg_time_specific / (tn_avg_time_specific * threads_N)
                row[f'{version_prefix} Parallel Efficiency'] = efficiency
            results_with_efficiency.append(row)
        
        return pd.DataFrame(results_with_efficiency) if results_with_efficiency else pd.DataFrame()

    def get_profiling_display_data(self, filters=None):
        filtered_agg_df = self.get_filtered_aggregated_data(filters)
        if filtered_agg_df.empty: return pd.DataFrame()

        melted_event_data = []
        event_col_pattern = re.compile(r"(CARTS|OMP) Event (.+?)(?::u|:k)? (Avg|Median|Min|Max|Stdev)")

        for _, row_series in filtered_agg_df.iterrows():
            row_dict = row_series.to_dict()
            common_data = {
                'Example Name': row_dict.get('Example Name'),
                'Problem Size': row_dict.get('Problem Size'),
                'Threads': row_dict.get('Threads'),
            }
            for col_name, value in row_dict.items():
                match = event_col_pattern.match(col_name)
                if match and pd.notna(value):
                    version, event_base_name, statistic = match.groups()
                    prefix_len = len(version) + len(" Event ")
                    suffix_len = len(statistic) +1 
                    full_event_name_in_col = col_name[prefix_len:-suffix_len]

                    melted_event_data.append({
                        **common_data,
                        'Version': version,
                        'Event Name': full_event_name_in_col.strip(), 
                        'Statistic': statistic,
                        'Value': pd.to_numeric(value, errors='coerce')
                    })
        
        if not melted_event_data: return pd.DataFrame()
        
        events_df_long = pd.DataFrame(melted_event_data)
        events_df_long.dropna(subset=['Value'], inplace=True)
        
        categorized_df = self._categorize_profiling_events(events_df_long)
        
        derived_metrics_list = []
        grouped_for_derived = categorized_df.groupby(
            ['Example Name', 'Problem Size', 'Threads', 'Version']
        )

        for group_key, group_df in grouped_for_derived:
            instr_series = group_df[
                (group_df['Event Name'].str.contains("instructions", case=False)) & 
                (group_df['Statistic'] == 'Avg')
            ]['Value']
            cycles_series = group_df[
                (group_df['Event Name'].str.contains("cycles", case=False)) & 
                (~group_df['Event Name'].str.contains("stall", case=False)) &
                (group_df['Statistic'] == 'Avg')
            ]['Value']

            if not instr_series.empty and not cycles_series.empty and cycles_series.iloc[0] > 0:
                ipc = instr_series.iloc[0] / cycles_series.iloc[0]
                derived_metrics_list.append({
                    'Example Name': group_key[0], 'Problem Size': group_key[1], 
                    'Threads': group_key[2], 'Version': group_key[3],
                    'Event Name': 'IPC', 'Statistic': 'Mean', 
                    'Value': ipc, 'category': 'CPU' 
                })

            misses_series = group_df[
                (group_df['Event Name'].str.contains("cache-misses", case=False)) & 
                (group_df['Statistic'] == 'Avg')
            ]['Value']
            refs_series = group_df[
                (group_df['Event Name'].str.contains("cache-references", case=False)) &
                (group_df['Statistic'] == 'Avg')
            ]['Value']
            
            if not misses_series.empty and not refs_series.empty and refs_series.iloc[0] > 0:
                miss_rate = misses_series.iloc[0] / refs_series.iloc[0]
                derived_metrics_list.append({
                    'Example Name': group_key[0], 'Problem Size': group_key[1], 
                    'Threads': group_key[2], 'Version': group_key[3],
                    'Event Name': 'Cache Miss Rate', 'Statistic': 'Mean',
                    'Value': miss_rate, 'category': 'Cache'
                })
        
        if derived_metrics_list:
            derived_df = pd.DataFrame(derived_metrics_list)
            final_profiling_df = pd.concat([categorized_df, derived_df], ignore_index=True)
            return final_profiling_df
        
        return categorized_df

    def _categorize_profiling_events(self, events_df_long):
        if events_df_long.empty: return events_df_long
        
        if 'Event Name' not in events_df_long.columns:
            print("Warning: 'Event Name' column not found for categorization.")
            events_df_long['category'] = 'Other'
            return events_df_long

        result_df = events_df_long.copy()
        categories = {
            'Cache': ['cache', 'L1-d', 'L1-i', 'L2', 'L3', 'LLC', 'l2_rqsts', 'l3_rqsts'],
            'Memory': ['mem_', 'memory', 'bus-cycle', 'remote_access', 'frontend_mem', 'membound'], 
            'CPU': ['cycle', 'instruction', 'branch', 'cpu-clock', 'task-clock', 'uops_issued', 'uops_retired', 'slots', 'int_misc', 'fp_assist'], 
            'TLB': ['tlb', 'dTLB', 'iTLB'],
            'Stalls': ['stall', 'resource_stall', 'cycle_activity.stall', 'lock'], 
            'Branch': ['branch'], 
            'Power': ['power/energy'],
        }
        
        result_df['category'] = 'Other'
        
        result_df.loc[result_df['Event Name'].str.contains('stall|lock_contention', case=False, na=False), 'category'] = 'Stalls'
        for pattern in categories['TLB']:
            result_df.loc[result_df['Event Name'].str.contains(pattern, case=False, na=False), 'category'] = 'TLB'
        for pattern in categories['Cache']:
            result_df.loc[(result_df['category'].isin(['Other', 'CPU', 'Memory'])) & result_df['Event Name'].str.contains(pattern, case=False, na=False), 'category'] = 'Cache'
        for pattern in categories['Memory']:
             result_df.loc[(result_df['category'].isin(['Other', 'CPU'])) & result_df['Event Name'].str.contains(pattern, case=False, na=False), 'category'] = 'Memory'
        for pattern in categories['Branch']:
            result_df.loc[(result_df['category'].isin(['Other', 'CPU'])) & result_df['Event Name'].str.contains(pattern, case=False, na=False), 'category'] = 'Branch'
        
        cpu_core_events = ['cycles:u', 'cpu-clock:u', 'task-clock:u', 'instructions:u', 'uops_issued.any:u', 'uops_retired.all:u']
        for cce in cpu_core_events:
            result_df.loc[result_df['Event Name'].str.contains(cce, case=False, na=False) & (result_df['category'] == 'Other'), 'category'] = 'CPU'
        
        for pattern in categories['CPU']:
            result_df.loc[(result_df['category'] == 'Other') & result_df['Event Name'].str.contains(pattern, case=False, na=False), 'category'] = 'CPU'
        for pattern in categories['Power']:
            result_df.loc[(result_df['category'] == 'Other') & result_df['Event Name'].str.contains(pattern, case=False, na=False), 'category'] = 'Power'
            
        return result_df

    def get_system_info(self):
        return self.system_info

    def get_overall_summary_stats(self, filters=None):
        summary = self.overall_stats_json.copy()
        summary_key_prefix = "Global" 

        filters_active = False
        if filters:
            available_opts = self.get_available_filter_options()
            if filters.get('examples') and len(filters['examples']) < len(available_opts.get('examples',[])):
                filters_active = True
            if filters.get('threads') and len(filters['threads']) < len(available_opts.get('threads',[])):
                filters_active = True
            if filters.get('sizes') and len(filters['sizes']) < len(available_opts.get('sizes',[])):
                filters_active = True
            if filters.get('correctness') and filters['correctness'] != 'ALL':
                filters_active = True
        
        if filters_active:
            summary_key_prefix = "Filtered" 
            filtered_agg_df = self.get_filtered_aggregated_data(filters)
            if not filtered_agg_df.empty:
                summary[f'{summary_key_prefix} Configs Compared'] = len(filtered_agg_df)
                
                if 'Speedup (OMP/CARTS)' in filtered_agg_df.columns:
                    speedups = pd.to_numeric(filtered_agg_df['Speedup (OMP/CARTS)'], errors='coerce').dropna()
                    if not speedups.empty:
                        summary[f'{summary_key_prefix} CARTS Faster Configs'] = (speedups > 1.05).sum()
                        summary[f'{summary_key_prefix} OMP Faster Configs'] = (speedups < 0.95).sum()
                        summary[f'{summary_key_prefix} Tied Configs'] = ((speedups >= 0.95) & (speedups <= 1.05)).sum()
                        
                        valid_speedups_gt_zero = speedups[speedups > 0]
                        if not valid_speedups_gt_zero.empty:
                            summary[f'{summary_key_prefix} Geo. Mean Speedup (OMP/CARTS)'] = np.exp(np.mean(np.log(valid_speedups_gt_zero)))
                        else:
                            summary[f'{summary_key_prefix} Geo. Mean Speedup (OMP/CARTS)'] = None
                        
                        max_s_filt_val = speedups.max()
                        min_s_filt_val = speedups.min()
                        max_s_filt_row = filtered_agg_df.loc[speedups.idxmax()] if pd.notna(max_s_filt_val) and not speedups.empty else None
                        min_s_filt_row = filtered_agg_df.loc[speedups.idxmin()] if pd.notna(min_s_filt_val) and not speedups.empty else None
                        
                        summary[f'{summary_key_prefix} Max Speedup'] = {
                            "value": max_s_filt_val if not speedups.empty else None,
                            "example_config": f"{max_s_filt_row['Example Name']} (S: {max_s_filt_row['Problem Size']}, T: {max_s_filt_row['Threads']})" if max_s_filt_row is not None else "N/A"
                        }
                        summary[f'{summary_key_prefix} Min Speedup'] = {
                            "value": min_s_filt_val if not speedups.empty else None,
                            "example_config": f"{min_s_filt_row['Example Name']} (S: {min_s_filt_row['Problem Size']}, T: {min_s_filt_row['Threads']})" if min_s_filt_row is not None else "N/A"
                        }
                    else: 
                        for stat in ['CARTS Faster Configs', 'OMP Faster Configs', 'Tied Configs', 'Geo. Mean Speedup (OMP/CARTS)']:
                            summary[f'{summary_key_prefix} {stat}'] = 0 if 'Configs' in stat else None
                        summary[f'{summary_key_prefix} Max Speedup'] = {"value": None, "example_config": "N/A"}
                        summary[f'{summary_key_prefix} Min Speedup'] = {"value": None, "example_config": "N/A"}
            else: 
                summary[f'{summary_key_prefix} Configs Compared'] = 0
                for stat in ['CARTS Faster Configs', 'OMP Faster Configs', 'Tied Configs', 'Geo. Mean Speedup (OMP/CARTS)']:
                     summary[f'{summary_key_prefix} {stat}'] = 0 if 'Configs' in stat else None
                summary[f'{summary_key_prefix} Max Speedup'] = {"value": None, "example_config": "N/A"}
                summary[f'{summary_key_prefix} Min Speedup'] = {"value": None, "example_config": "N/A"}

        if filters_active:
            original_keys = list(self.overall_stats_json.keys()) 
            for k_original in original_keys: 
                if not k_original.startswith("Global") and not k_original.startswith("Filtered"): 
                    summary[f"Global {k_original.replace('_', ' ').title()}"] = summary.pop(k_original) 
        else: 
            original_keys = list(self.overall_stats_json.keys())
            for k_original in original_keys:
                 if not k_original.startswith("Global") and not k_original.startswith("Filtered"):
                    summary[f"Global {k_original.replace('_', ' ').title()}"] = summary.pop(k_original)
        return summary

    def get_available_filter_options(self):
        options = {
            'examples': [], 'threads': [], 'sizes': [],
            'correctness': [
                {'label': 'All', 'value': 'ALL'},
                {'label': 'Correct', 'value': 'CORRECT'},
                {'label': 'Incorrect', 'value': 'INCORRECT'},
                {'label': 'Timeout', 'value': 'TIMEOUT'},
                {'label': 'Error Runtime', 'value': 'ERROR_RUNTIME'},
                {'label': 'Unknown', 'value': 'UNKNOWN'},
            ]
        }
        if not self.df_aggregated.empty:
            if 'Example Name' in self.df_aggregated.columns:
                options['examples'] = [{'label': str(ex), 'value': ex} for ex in sorted(self.df_aggregated['Example Name'].unique())]
            if 'Threads' in self.df_aggregated.columns:
                unique_threads = pd.to_numeric(self.df_aggregated['Threads'], errors='coerce').dropna().unique()
                options['threads'] = [{'label': str(int(t)), 'value': int(t)} for t in sorted(unique_threads)]
            if 'Problem Size' in self.df_aggregated.columns:
                options['sizes'] = [{'label': str(s), 'value': s} for s in sorted(self.df_aggregated['Problem Size'].dropna().unique())]
        
        return options

# --- PlotManager Class ---
class PlotManager:
    def __init__(self, plot_template, data_manager, help_manager):
        self.template = plot_template
        self.dm = data_manager 
        self.help_manager = help_manager

    def get_scaling_plot(self, filters=None):
        benchmark_data = self.dm.get_benchmark_display_data(filters)
        return create_scaling_plot(benchmark_data, self.template)

    def get_efficiency_plot(self, filters=None):
        benchmark_data = self.dm.get_benchmark_display_data(filters)
        return create_parallel_efficiency_plot(benchmark_data, self.template)

    def get_speedup_plot(self, filters=None):
        benchmark_data = self.dm.get_benchmark_display_data(filters)
        return create_speedup_comparison_plot(benchmark_data, self.template)

    def get_slowdown_plot(self, filters=None):
        benchmark_data = self.dm.get_benchmark_display_data(filters)
        return create_slowdown_comparison_plot(benchmark_data, self.template)

    def get_variability_plot_components(self, filters=None):
        raw_benchmark_df = self.dm.get_filtered_raw_benchmark_data(filters)
        return create_execution_time_variability_plot_components(raw_benchmark_df, self.template, self.help_manager)

    def get_cache_plot(self, filters=None):
        profiling_data = self.dm.get_profiling_display_data(filters)
        return create_cache_performance_comparison(profiling_data, self.template)

    def get_ipc_plot(self, filters=None):
        profiling_data = self.dm.get_profiling_display_data(filters)
        return create_ipc_comparison_plot(profiling_data, self.template)

    def get_stalls_plot(self, filters=None):
        profiling_data = self.dm.get_profiling_display_data(filters)
        return create_stalls_analysis_plot(profiling_data, self.template)

    def get_generic_profiling_plot(self, category, filters=None):
        profiling_data = self.dm.get_profiling_display_data(filters)
        category_df = profiling_data[profiling_data['category'] == category]
        
        if category_df.empty:
            fig = go.Figure().add_annotation(text=f"No data for profiling category: {category}", x=0.5, y=0.5, showarrow=False)
            return self.template.apply(fig, plot_title=f"{category} Analysis", height=500), "" 

        first_event = "N/A"
        if not category_df['Event Name'].empty:
            event_counts = category_df['Event Name'].value_counts()
            preferred_events = {
                "Memory": ["mem_inst_retired.all_loads:u", "mem_inst_retired.all_stores:u"],
                "Branch": ["branch-instructions:u", "branch-misses:u"]
            }
            
            if category in preferred_events:
                for pe in preferred_events[category]:
                    if pe in event_counts.index:
                        first_event = pe
                        break
            if first_event == "N/A": 
                 first_event = event_counts.index[0] if not event_counts.empty else category_df['Event Name'].unique()[0]
            
            plot_df = category_df[
                (category_df['Event Name'] == first_event) & 
                (category_df['Statistic'].isin(['Avg', 'Mean']))
            ]

            fig = go.Figure()
            if not plot_df.empty:
                for (example, version, p_size), group_data in plot_df.groupby(['Example Name', 'Version', 'Problem Size']):
                    group_data = group_data.sort_values('Threads')
                    if 'Threads' in group_data and 'Value' in group_data and not group_data['Value'].isnull().all():
                        current_color = PRIMARY_COLOR if version == 'CARTS' else ACCENT_COLOR
                        current_dash = 'solid' if version == 'CARTS' else 'dash'
                        fig.add_trace(go.Scatter(
                            x=group_data['Threads'], y=group_data['Value'], name=f"{example} (S:{p_size}) - {version}",
                            mode='lines+markers',
                            line=dict(color=current_color, width=2, dash=current_dash),
                            marker=dict(size=8, color=current_color, line=dict(width=1, color=MARKER_OUTLINE_COLOR)),
                            hovertemplate=f"<b>{example} - {version} (S:{p_size})</b><br>Event: {first_event.split(':')[0]}<br>Threads: %{{x}}<br>Value: %{{y:,.2f}}<extra></extra>"
                        ))
            if not fig.data:
                fig.add_annotation(text=f"No plottable data for event: {first_event} in {category}", x=0.5, y=0.5, showarrow=False)
            
            yaxis_type = 'linear'
            if not plot_df.empty and 'Value' in plot_df.columns:
                valid_values = plot_df['Value'].dropna()
                if not valid_values.empty:
                    min_val_pos = valid_values[valid_values > 0].min()
                    max_val = valid_values.max()
                    if pd.notna(min_val_pos) and pd.notna(max_val) and max_val / min_val_pos > 100:
                        yaxis_type = 'log'
            
            specific_event_help = f"- Shows mean/average value for event: `{first_event.split(':')[0]}`."
            
            return self.template.apply(fig, plot_title=f"{category} Analysis: {first_event.split(':')[0]}",
                                         xaxis_title="Threads", yaxis_title=f"Value ({yaxis_type})", height=550, legend_title="Ex (Size) - Ver").update_yaxes(type=yaxis_type), specific_event_help
        else:
            fig = go.Figure().add_annotation(text=f"No specific events found for category: {category}", x=0.5, y=0.5, showarrow=False)
            return self.template.apply(fig, plot_title=f"{category} Analysis", height=500), ""

# --- TableManager Class ---
class TableManager:
    def __init__(self, font_family):
        self.font_family = font_family

    def create_benchmark_summary_table(self, benchmark_display_df):
        if benchmark_display_df.empty:
            return html.Div("No benchmarking data to display after filtering.", style={'color': ACCENT_COLOR, 'fontWeight': 'bold', 'padding': '12px'})
        
        cols_to_display = [
            "Example Name", "Problem Size", "Threads",
            "CARTS Avg Time (s)", "OMP Avg Time (s)",
            "Speedup (OMP/CARTS)",
            "CARTS Parallel Efficiency", "OMP Parallel Efficiency",
            "CARTS Correctness", "OMP Correctness",
            "CARTS Config Threads", "CARTS Config Nodes" 
        ]
        display_df_filtered_cols = benchmark_display_df[[col for col in cols_to_display if col in benchmark_display_df.columns]].copy()

        for col in display_df_filtered_cols.columns:
            if "Time (s)" in col or "Speedup" in col or "Efficiency" in col :
                display_df_filtered_cols[col] = display_df_filtered_cols[col].apply(lambda x: format_number(x, 4))
            elif "Threads" == col or "Config Threads" in col or "Config Nodes" in col:
                 display_df_filtered_cols[col] = display_df_filtered_cols[col].apply(lambda x: format_number(x, 0))

        data_for_table = display_df_filtered_cols.to_dict('records')
        columns_for_table = [{"name": col_name, "id": col_name} for col_name in display_df_filtered_cols.columns]

        return dash_table.DataTable(
            data=data_for_table, columns=columns_for_table, page_size=10,
            style_table={'overflowX': 'auto', 'borderRadius': '8px', 'boxShadow': '0 2px 8px rgba(0,0,0,0.07)'},
            style_cell={**StyleManager.TABLE_CELL_STYLE},
            style_header={**StyleManager.TABLE_HEADER_STYLE},
            style_cell_conditional=[
                {'if': {'column_id': c}, 'textAlign': 'right'} for c in display_df_filtered_cols.columns if 'Time' in c or 'Speedup' in c or 'Efficiency' in c or 'Threads' in c or 'Nodes' in c
            ] + [
                {'if': {'column_id': c}, 'textAlign': 'left'} for c in ["Example Name", "Problem Size"]
            ] + [
                {'if': {'column_id': c}, 'textAlign': 'center'} for c in display_df_filtered_cols.columns if 'Correctness' in c
            ],
            style_data_conditional=[
                {'if': {'column_id': col, 'filter_query': f'{{{col}}} = CORRECT'}, 'backgroundColor': '#d4edda', 'color': '#155724'} for col in ['CARTS Correctness', 'OMP Correctness'] 
            ] + [
                {'if': {'column_id': col, 'filter_query': f'{{{col}}} != CORRECT && {{{col}}} != "N/A" && {{{col}}} != "-"'}, 'backgroundColor': '#f8d7da', 'color': '#721c24'} for col in ['CARTS Correctness', 'OMP Correctness'] 
            ] + [
                {'if': {'column_id': 'Speedup (OMP/CARTS)', 'filter_query': '{Speedup (OMP/CARTS)} > 1.05'}, 'color': SUCCESS_COLOR, 'fontWeight': 'bold'},
                {'if': {'column_id': 'Speedup (OMP/CARTS)', 'filter_query': '{Speedup (OMP/CARTS)} < 0.95'}, 'color': ERROR_COLOR, 'fontWeight': 'bold'},
            ],
            sort_action='native', filter_action='native', style_as_list_view=True,
        )

    def create_profiling_summary_table(self, profiling_display_df):
        if profiling_display_df.empty:
            return html.Div("No profiling statistics to display after filtering or processing.", style={'padding': '10px', 'color': WARNING_COLOR})

        cols_to_show = ['Example Name', 'Problem Size', 'Threads', 'Version', 'Event Name', 'Statistic', 'Value', 'category']
        table_df = profiling_display_df[[col for col in cols_to_show if col in profiling_display_df.columns]].copy()
        
        if 'Value' in table_df.columns:
            table_df['Value_Formatted'] = table_df['Value'].apply(lambda x: format_number(x, 4)) 
            is_rate_ipc = table_df['Event Name'].str.contains("Rate|IPC", case=False, na=False)
            table_df.loc[is_rate_ipc, 'Value_Formatted'] = table_df.loc[is_rate_ipc, 'Value'].apply(lambda x: format_number(x, 3))
            is_large_count = table_df['Value'] > 1000 
            table_df.loc[is_large_count & ~is_rate_ipc, 'Value_Formatted'] = table_df.loc[is_large_count & ~is_rate_ipc, 'Value'].apply(lambda x: format_number(x, 0))
            table_df['Value'] = table_df['Value_Formatted']
            table_df.drop(columns=['Value_Formatted'], inplace=True)

        data_for_table = table_df.to_dict('records')
        columns_for_table = [{"name": col_name, "id": col_name} for col_name in table_df.columns]

        return dash_table.DataTable(
            data=data_for_table, columns=columns_for_table, page_size=15,
            style_table={'overflowX': 'auto', 'borderRadius': '8px', 'boxShadow': '0 2px 8px rgba(0,0,0,0.07)'},
            style_cell={**StyleManager.TABLE_CELL_STYLE, 'minWidth': '80px', 'maxWidth': '200px', 'overflow': 'hidden', 'textOverflow': 'ellipsis'},
            style_header={**StyleManager.TABLE_HEADER_STYLE},
            style_cell_conditional=[
                {'if': {'column_id': c}, 'textAlign': 'right'} for c in ['Threads', 'Value']
            ] + [
                {'if': {'column_id': c}, 'textAlign': 'left'} for c in ['Example Name', 'Problem Size', 'Event Name', 'category']
            ] + [
                 {'if': {'column_id': c}, 'textAlign': 'center'} for c in ['Version', 'Statistic']
            ],
            sort_action='native', filter_action='native', style_as_list_view=True,
            tooltip_data=[
                {column: {'value': str(value), 'type': 'markdown'} for column, value in row.items()}
                for row in data_for_table
            ],
            tooltip_duration=None
        )

# --- Plotting functions ---
def create_scaling_plot(benchmark_display_df, template):
    if benchmark_display_df.empty:
        fig = go.Figure().add_annotation(text="No scaling data available", x=0.5, y=0.5, showarrow=False)
        return template.apply(fig, plot_title="Execution Time Scaling", height=400)

    examples = sorted(benchmark_display_df['Example Name'].unique())
    num_examples = len(examples)
    if num_examples == 0:
        fig = go.Figure().add_annotation(text="No examples for scaling plot", x=0.5, y=0.5, showarrow=False)
        return template.apply(fig, plot_title="Execution Time Scaling", height=400)
    
    cols = min(3, num_examples); rows = (num_examples + cols - 1) // cols
    fig = make_subplots(rows=rows, cols=cols, subplot_titles=[f"Ex: {ex}" for ex in examples],
                        shared_xaxes=True, shared_yaxes=False, vertical_spacing=0.22, horizontal_spacing=0.08)
    legend_added = set()

    for i, example in enumerate(examples):
        row_idx, col_idx = i // cols + 1, i % cols + 1
        example_data = benchmark_display_df[benchmark_display_df['Example Name'] == example]
        problem_sizes = sorted(example_data['Problem Size'].unique())

        for ps_idx, problem_size in enumerate(problem_sizes):
            size_group = example_data[example_data['Problem Size'] == problem_size]
            if size_group.empty: continue

            for version_prefix, time_col, line_dash, color in [
                ("CARTS", "CARTS Avg Time (s)", "solid", PRIMARY_COLOR),
                ("OMP", "OMP Avg Time (s)", "dash", ACCENT_COLOR)
            ]:
                if time_col not in size_group.columns: continue
                ver_group = size_group[['Threads', time_col]].copy()
                ver_group['Threads'] = pd.to_numeric(ver_group['Threads'], errors='coerce')
                ver_group[time_col] = pd.to_numeric(ver_group[time_col], errors='coerce')
                ver_group.dropna(subset=['Threads', time_col], inplace=True)
                ver_group.sort_values('Threads', inplace=True)

                if ver_group.empty: continue
                
                legend_name = f"{version_prefix} (S: {problem_size})"
                show_legend_trace = legend_name not in legend_added
                if show_legend_trace: legend_added.add(legend_name)

                fig.add_trace(go.Scatter(
                    x=ver_group['Threads'], y=ver_group[time_col], mode='lines+markers', name=legend_name,
                    line=dict(color=color, width=2, dash=line_dash),
                    marker=dict(size=8, color=color, line=dict(width=1, color=MARKER_OUTLINE_COLOR)),
                    legendgroup=legend_name, showlegend=show_legend_trace,
                    hovertemplate=f"<b>{example} - {version_prefix}</b><br>Size: {problem_size}<br>Threads: %{{x}}<br>Time: %{{y:.4f}} s<extra></extra>"
                ), row=row_idx, col=col_idx)
        
        fig.update_xaxes(title_text="Threads", type='category', row=row_idx, col=col_idx, title_standoff=5)
        fig.update_yaxes(title_text="Avg Time (s)", type='log', row=row_idx, col=col_idx, title_standoff=5)

    calculated_height = max(500, 300 * rows)
    fig = template.apply(fig, plot_title="Execution Time Scaling by Example",
                        height=calculated_height, legend_title="Version (Size)",
                        xaxis_title="Threads", yaxis_title="Avg Time (s)")
    # Ensure all subplots have consistent style
    for i in range(1, rows * cols + 1):
        xaxis_name = f'xaxis{i}' if i > 1 else 'xaxis'
        yaxis_name = f'yaxis{i}' if i > 1 else 'yaxis'
        fig.layout[xaxis_name].update(
            title_font=dict(size=template.xaxis_title_size, color=NEUTRAL_COLOR, family=template.font_family),
            tickfont=dict(size=template.font_size - 2, family=template.font_family, color=NEUTRAL_COLOR),
            gridcolor=template.grid_color, showgrid=True,
            showline=True, linecolor=template.axis_color, ticks='outside', automargin=True
        )
        fig.layout[yaxis_name].update(
            title_font=dict(size=template.xaxis_title_size, color=NEUTRAL_COLOR, family=template.font_family),
            tickfont=dict(size=template.font_size - 2, family=template.font_family, color=NEUTRAL_COLOR),
            gridcolor=template.grid_color, showgrid=True,
            showline=True, linecolor=template.axis_color, ticks='outside', automargin=True
        )
    return fig

def create_speedup_comparison_plot(benchmark_display_df, template):
    speedup_col = 'Speedup (OMP/CARTS)'
    if benchmark_display_df.empty or speedup_col not in benchmark_display_df.columns:
        fig = go.Figure().add_annotation(text="No speedup data available", x=0.5, y=0.5, showarrow=False)
        return template.apply(fig, plot_title="OMP vs CARTS Speedup", height=500)

    df_to_plot = benchmark_display_df.copy()
    df_to_plot[speedup_col] = pd.to_numeric(df_to_plot[speedup_col], errors='coerce')
    df_to_plot.dropna(subset=[speedup_col], inplace=True)

    if df_to_plot.empty:
        fig = go.Figure().add_annotation(text="No valid speedup values to plot", x=0.5, y=0.5, showarrow=False)
        return template.apply(fig, plot_title="OMP vs CARTS Speedup", height=500)

    fig = go.Figure()
    fig.add_shape(type="line", x0=0, y0=1, x1=1, y1=1, line=dict(color="gray", width=1, dash="dash"), xref="paper", yref="y")

    for (example, problem_size), group in df_to_plot.groupby(['Example Name', 'Problem Size']):
        group = group.sort_values('Threads')
        if group.empty: continue

        carts_time_series = pd.to_numeric(group.get('CARTS Avg Time (s)'), errors='coerce').apply(lambda x: format_number(x,4))
        omp_time_series = pd.to_numeric(group.get('OMP Avg Time (s)'), errors='coerce').apply(lambda x: format_number(x,4))
        
        customdata_stack = np.stack((
            group['Problem Size'].astype(str),
            omp_time_series,
            carts_time_series
        ), axis=-1)

        fig.add_trace(go.Scatter(
            x=group['Threads'], y=group[speedup_col],
            mode='lines+markers', name=f"{example} (S: {problem_size})",
            line=dict(color=PRIMARY_COLOR, width=2),
            marker=dict(size=8, color=PRIMARY_COLOR, line=dict(width=1, color=MARKER_OUTLINE_COLOR)),
            customdata=customdata_stack,
            hovertemplate=(
                f"<b>{example}</b> (Size: %{{customdata[0]}})<br>" +
                "Threads: %{x}<br>" +
                "OMP/CARTS Speedup: %{y:.2f}x<br>" +
                "OMP Avg Time: %{customdata[1]}s<br>" +
                "CARTS Avg Time: %{customdata[2]}s<extra></extra>"
            )
        ))
    
    y_max_data = df_to_plot[speedup_col].max()
    y_min_data = df_to_plot[speedup_col].min()
    y_upper_plot_limit = max(1.5, y_max_data * 1.1) if pd.notna(y_max_data) else 1.5
    y_lower_plot_limit = min(0.5, y_min_data * 0.9) if pd.notna(y_min_data) else 0.5

    fig.update_layout(
        shapes=[dict(type="rect", xref="paper", yref="y", x0=0,y0=y_lower_plot_limit,x1=1,y1=1,fillcolor="rgba(255,0,0,0.05)",layer="below",line_width=0), 
                dict(type="rect", xref="paper", yref="y", x0=0,y0=1,x1=1,y1=y_upper_plot_limit,fillcolor="rgba(0,255,0,0.05)",layer="below",line_width=0)], 
        annotations=[dict(x=0.95,y=min(0.8, y_min_data*1.1 if pd.notna(y_min_data) and y_min_data < 1 else 0.8),xref="paper",yref="y",text="CARTS Slower",showarrow=False,font=dict(size=12,color=ERROR_COLOR)),
                     dict(x=0.95,y=max(1.2, y_max_data*0.9 if pd.notna(y_max_data) and y_max_data > 1 else 1.2),xref="paper",yref="y",text="CARTS Faster",showarrow=False,font=dict(size=12,color=SUCCESS_COLOR))],
        xaxis=dict(type='category'),
        yaxis_range=[y_lower_plot_limit, y_upper_plot_limit]
    )
    return template.apply(fig, plot_title="OMP vs CARTS Performance (OMP Avg Time / CARTS Avg Time)",
                          xaxis_title="Threads", yaxis_title="Speedup (OMP/CARTS)",
                          height=550, legend_title="Example (Size)")

def create_slowdown_comparison_plot(benchmark_display_df, template):
    speedup_col = 'Speedup (OMP/CARTS)'
    if benchmark_display_df.empty or speedup_col not in benchmark_display_df.columns:
        fig = go.Figure().add_annotation(text="No data for slowdown comparison", x=0.5, y=0.5, showarrow=False)
        return template.apply(fig, plot_title="CARTS vs OMP Slowdown", height=500)

    df_to_plot = benchmark_display_df.copy()
    df_to_plot[speedup_col] = pd.to_numeric(df_to_plot[speedup_col], errors='coerce')
    df_to_plot.dropna(subset=[speedup_col], inplace=True)
    df_to_plot['Slowdown (CARTS/OMP)'] = df_to_plot[speedup_col].apply(lambda x: 1/x if x != 0 else np.nan)
    df_to_plot.replace([np.inf, -np.inf], np.nan, inplace=True)
    df_to_plot.dropna(subset=['Slowdown (CARTS/OMP)'], inplace=True)

    if df_to_plot.empty:
        fig = go.Figure().add_annotation(text="No valid slowdown values to plot", x=0.5, y=0.5, showarrow=False)
        return template.apply(fig, plot_title="CARTS vs OMP Slowdown", height=500)

    fig = go.Figure()
    fig.add_shape(type="line", x0=0, y0=1, x1=1, y1=1, line=dict(color="gray", width=1, dash="dash"), xref="paper", yref="y")

    for (example, problem_size), group in df_to_plot.groupby(['Example Name', 'Problem Size']):
        group = group.sort_values('Threads')
        if group.empty: continue
        
        carts_time_series = pd.to_numeric(group.get('CARTS Avg Time (s)'), errors='coerce').apply(lambda x: format_number(x,4))
        omp_time_series = pd.to_numeric(group.get('OMP Avg Time (s)'), errors='coerce').apply(lambda x: format_number(x,4))

        customdata_stack = np.stack((
            group['Problem Size'].astype(str),
            carts_time_series,
            omp_time_series
        ), axis=-1)

        fig.add_trace(go.Scatter(
            x=group['Threads'], y=group['Slowdown (CARTS/OMP)'],
            mode='lines+markers', name=f"{example} (S: {problem_size})",
            line=dict(color=ACCENT_COLOR, width=2, dash='dash'), 
            marker=dict(size=8, color=ACCENT_COLOR, line=dict(width=1, color=MARKER_OUTLINE_COLOR)),
            customdata=customdata_stack,
            hovertemplate=(
                f"<b>{example}</b> (Size: %{{customdata[0]}})<br>" +
                "Threads: %{x}<br>" +
                "CARTS/OMP Slowdown: %{y:.2f}x<br>" + 
                "CARTS Avg Time: %{customdata[1]}s<br>" +
                "OMP Avg Time: %{customdata[2]}s<extra></extra>"
            )
        ))
    
    y_max_data = df_to_plot['Slowdown (CARTS/OMP)'].max()
    y_min_data = df_to_plot['Slowdown (CARTS/OMP)'].min()
    y_upper_plot_limit = max(1.5, y_max_data * 1.1) if pd.notna(y_max_data) else 1.5
    y_lower_plot_limit = min(0.5, y_min_data * 0.9) if pd.notna(y_min_data) else 0.5

    fig.update_layout(
        shapes=[dict(type="rect", xref="paper", yref="y", x0=0,y0=y_lower_plot_limit,x1=1,y1=1,fillcolor="rgba(0,255,0,0.05)",layer="below",line_width=0), 
                dict(type="rect", xref="paper", yref="y", x0=0,y0=1,x1=1,y1=y_upper_plot_limit,fillcolor="rgba(255,0,0,0.05)",layer="below",line_width=0)], 
        annotations=[dict(x=0.95,y=min(0.8, y_min_data*1.1 if pd.notna(y_min_data) and y_min_data < 1 else 0.8),xref="paper",yref="y",text="CARTS Faster",showarrow=False,font=dict(size=12,color=SUCCESS_COLOR)),
                     dict(x=0.95,y=max(1.2, y_max_data*0.9 if pd.notna(y_max_data) and y_max_data > 1 else 1.2),xref="paper",yref="y",text="CARTS Slower",showarrow=False,font=dict(size=12,color=ERROR_COLOR))],
        xaxis=dict(type='category'),
        yaxis_range=[y_lower_plot_limit, y_upper_plot_limit]
    )
    return template.apply(fig, plot_title="CARTS vs OMP Performance (CARTS Avg Time / OMP Avg Time)",
                          xaxis_title="Threads", yaxis_title="Slowdown (CARTS/OMP)",
                          height=550, legend_title="Example (Size)")

def create_parallel_efficiency_plot(benchmark_display_df, template):
    if benchmark_display_df.empty:
        fig = go.Figure().add_annotation(text="No parallel efficiency data", x=0.5, y=0.5, showarrow=False)
        return template.apply(fig, plot_title="Parallel Efficiency", height=500)

    df_plot = benchmark_display_df.copy()
    df_plot['Threads'] = pd.to_numeric(df_plot['Threads'], errors='coerce')
    df_plot = df_plot[df_plot['Threads'] > 1]
    
    if df_plot.empty:
        fig = go.Figure().add_annotation(text="No multi-threaded data for efficiency plot", x=0.5, y=0.5, showarrow=False)
        return template.apply(fig, plot_title="Parallel Efficiency", height=500)

    fig = go.Figure()
    max_threads_val = df_plot['Threads'].max()
    if pd.notna(max_threads_val) and max_threads_val >= 2:
        thread_ticks = sorted(df_plot['Threads'].unique())
        if thread_ticks:
            fig.add_trace(go.Scatter(x=thread_ticks, y=[1.0]*len(thread_ticks),
                                 mode='lines', name='Ideal Scaling', line=dict(color='grey', dash='dash', width=1.5)))

    for (example, problem_size), group_ex_ps in df_plot.groupby(['Example Name', 'Problem Size']):
        for version_prefix, eff_col, line_dash, color in [
            ("CARTS", "CARTS Parallel Efficiency", "solid", PRIMARY_COLOR),
            ("OMP", "OMP Parallel Efficiency", "dash", ACCENT_COLOR)
        ]:
            if eff_col not in group_ex_ps.columns: continue
            
            group_ver = group_ex_ps[['Threads', eff_col]].copy()
            group_ver[eff_col] = pd.to_numeric(group_ver[eff_col], errors='coerce')
            group_ver.dropna(subset=['Threads', eff_col], inplace=True)
            group_ver.sort_values('Threads', inplace=True)

            if group_ver.empty: continue

            fig.add_trace(go.Scatter(
                x=group_ver['Threads'], y=group_ver[eff_col], mode='lines+markers',
                name=f"{example} - {version_prefix} (S: {problem_size})",
                line=dict(color=color, width=2, dash=line_dash),
                marker=dict(size=8, color=color, line=dict(width=1, color=MARKER_OUTLINE_COLOR)),
                hovertemplate=f"<b>{example} - {version_prefix}</b> (S: {problem_size})<br>Threads: %{{x}}<br>Efficiency: %{{y:.3f}}<extra></extra>"
            ))
    
    all_eff_values = pd.concat([
        pd.to_numeric(df_plot['CARTS Parallel Efficiency'], errors='coerce').dropna() if 'CARTS Parallel Efficiency' in df_plot else pd.Series(dtype=float),
        pd.to_numeric(df_plot['OMP Parallel Efficiency'], errors='coerce').dropna() if 'OMP Parallel Efficiency' in df_plot else pd.Series(dtype=float)
    ])
    max_eff = all_eff_values.max() if not all_eff_values.empty else 1.1
    
    fig.update_layout(yaxis_range=[0, max(1.1, max_eff * 1.05 if pd.notna(max_eff) else 1.1)], xaxis=dict(type='category'))
    return template.apply(fig, plot_title="Parallel Efficiency (Strong Scaling)", xaxis_title="Threads",
                          yaxis_title="Efficiency", height=550, legend_title="Ex - Ver (Size)")

def create_execution_time_variability_plot_components(raw_benchmark_df, template, help_manager):
    if raw_benchmark_df.empty or 'times_seconds' not in raw_benchmark_df.columns:
        return [html.Div("No raw benchmark data for variability plots.", style={'padding':'10px', 'textAlign':'center', 'color': WARNING_COLOR})], None

    records = []
    for _, row_series in raw_benchmark_df.iterrows():
        row = row_series.to_dict()
        current_times = row.get('times_seconds', []) 
        if not isinstance(current_times, list): current_times = [] 

        for t_val in current_times:
            records.append({
                'example_name': row.get('example_name', 'Unknown Example'),
                'problem_size': str(row.get('problem_size', 'N/A')),
                'threads': row.get('threads', 1), 
                'version': row.get('version', 'Unknown Version'),
                'time': t_val 
            })
    
    flat_df_for_variability = pd.DataFrame(records)
    if flat_df_for_variability.empty:
        return [html.Div("No timing data points to plot variability.", style={'padding': '10px', 'textAlign': 'center', 'color': WARNING_COLOR})], None

    flat_df_for_variability['threads'] = pd.to_numeric(flat_df_for_variability['threads'], errors='coerce').fillna(0).astype(int)
    unique_examples = sorted(flat_df_for_variability['example_name'].unique())

    if not unique_examples:
        return [html.Div("No examples available for variability plots.", style={'padding':'10px', 'textAlign':'center', 'color': WARNING_COLOR})], None

    sub_tabs_children = []
    base_variability_help = help_manager.get_help_text("variability_base")

    for example_name in unique_examples:
        example_specific_df = flat_df_for_variability[flat_df_for_variability['example_name'] == example_name]
        fig_content = None
        sanitized_example_name = re.sub(r'\W+', '_', example_name)

        if example_specific_df.empty or 'time' not in example_specific_df.columns or example_specific_df['time'].isnull().all():
            fig_content = html.Div(f"No plottable variability data for example: {example_name}", style={'padding':'10px', 'textAlign':'center'})
        else:
            fig = px.box(example_specific_df, x='threads', y='time', color='version',
                         facet_col='problem_size', facet_col_wrap=2, 
                         boxmode='group',
                         labels={'time': 'Execution Time (s)', 'threads': 'Threads', 'version': 'Version', 'problem_size': 'Problem Size'},
                         category_orders={'threads': sorted(example_specific_df['threads'].unique())},
                         color_discrete_map={'CARTS': PRIMARY_COLOR, 'OMP': ACCENT_COLOR}
                        )
            fig.update_yaxes(type='log', matches=None, title_standoff=10) 
            fig.update_xaxes(matches=None, type='category', title_standoff=10) 
            
            num_problem_sizes = example_specific_df['problem_size'].nunique()
            plot_height = max(450, 250 * ((num_problem_sizes + 1) // 2) ) 

            fig = template.apply(fig, plot_title=f"Variability: {example_name}",
                                 height=plot_height, legend_title="Version",
                                 xaxis_title="Threads", yaxis_title="Execution Time (s)") 
            fig.for_each_annotation(lambda a: a.update(text=a.text.split("=")[-1])) 
            fig.update_layout(margin=dict(t=60, b=120 if template.legend_y < 0 else 80))

            graph_component = dcc.Graph(
                id=f'variability-graph-{sanitized_example_name}',
                figure=fig,
                className='resizable-variability-plot', 
                style=graph_style 
            )
            fig_content = create_plot_with_help(graph_component, base_variability_help, f"variability-{sanitized_example_name}")
        
        sub_tabs_children.append(dcc.Tab(label=example_name, value=sanitized_example_name, children=[fig_content]))

    if not sub_tabs_children:
        return [html.Div("No variability data to display after filtering.", style={'padding':'10px', 'textAlign':'center', 'color': WARNING_COLOR})], None

    default_active_sub_tab = re.sub(r'\W+', '_', unique_examples[0]) if unique_examples else None
    return sub_tabs_children, default_active_sub_tab

def create_cache_performance_comparison(profiling_display_df, template):
    if profiling_display_df.empty:
        fig = go.Figure().add_annotation(text="No cache profiling data", x=0.5, y=0.5, showarrow=False)
        return template.apply(fig, plot_title="Cache Performance", height=500), "" 

    cache_miss_rate_df = profiling_display_df[
        (profiling_display_df['Event Name'] == 'Cache Miss Rate') &
        (profiling_display_df['Statistic'] == 'Mean') 
    ].copy()

    specific_help = ""
    if not cache_miss_rate_df.empty:
        fig = go.Figure()
        for (example, problem_size), group_data in cache_miss_rate_df.groupby(['Example Name', 'Problem Size']):
            for version in ['CARTS', 'OMP']:
                ver_data = group_data[group_data['Version'] == version].sort_values('Threads')
                if ver_data.empty or ver_data['Value'].isnull().all(): continue
                current_color = PRIMARY_COLOR if version == 'CARTS' else ACCENT_COLOR
                fig.add_trace(go.Bar(
                    x=ver_data['Threads'], y=ver_data['Value'] * 100, name=f"{example} (S:{problem_size}) - {version}",
                    marker_color=current_color, opacity=0.85,
                    marker_line_width=template.bar_line_width, marker_line_color=current_color,
                    hovertemplate=f"<b>{example} (S:{problem_size}) - {version}</b><br>Threads: %{{x}}<br>Cache Miss Rate: %{{y:.2f}}%<extra></extra>"
                ))
        fig.update_layout(barmode='group', xaxis_type='category')
        specific_help = "- Shows L1/L2 cache miss rate (derived from `cache-misses` / `cache-references`)."
        return template.apply(fig, plot_title="Cache Miss Rate Comparison", xaxis_title="Threads",
                              yaxis_title="Cache Miss Rate (%)", height=550, legend_title="Ex (Size) - Ver"), specific_help
    else: 
        fallback_event = 'cache-misses:u' 
        df_cache_fallback = profiling_display_df[
            (profiling_display_df['Event Name'].str.contains(fallback_event, case=False)) &
            (profiling_display_df['Statistic'] == 'Avg') 
        ].copy()

        if df_cache_fallback.empty:
            fig = template.apply(go.Figure().add_annotation(text="No specific cache data (miss rate or raw counts)", x=0.5,y=0.5,showarrow=False), plot_title="Cache Performance", height=500)
            return fig, "- No specific cache data (miss rate or raw counts like 'cache-misses:u') found."
        
        event_to_plot_name = df_cache_fallback['Event Name'].unique()[0] 
        fig = go.Figure()
        for (example, problem_size, version), group_data in df_cache_fallback.groupby(['Example Name', 'Problem Size', 'Version']):
            group_data = group_data.sort_values('Threads')
            if group_data.empty or group_data['Value'].isnull().all(): continue
            current_color = PRIMARY_COLOR if version == 'CARTS' else ACCENT_COLOR
            current_dash = 'solid' if version == 'CARTS' else 'dash'
            fig.add_trace(go.Scatter(
                x=group_data['Threads'], y=group_data['Value'], name=f"{example} (S:{problem_size}) - {version}",
                mode='lines+markers',
                line=dict(color=current_color, dash=current_dash, width=2),
                marker=dict(color=current_color, size=8, line=dict(width=1, color=MARKER_OUTLINE_COLOR)),
                hovertemplate=f"<b>{example} ({version})</b><br>Threads: %{{x}}<br>{event_to_plot_name.split(':')[0]}: %{{y:,.0f}}<extra></extra>"
            ))
        fig.update_layout(xaxis_type='category', yaxis_type='log')
        specific_help = f"- Shows raw counts for event: `{event_to_plot_name.split(':')[0]}` (log scale) as 'Cache Miss Rate' was not available."
        return template.apply(fig, plot_title=f"{event_to_plot_name.split(':')[0]} Comparison", xaxis_title="Threads",
                              yaxis_title="Count (log)", height=550, legend_title="Ex (Size) - Ver"), specific_help

def create_ipc_comparison_plot(profiling_display_df, template):
    if profiling_display_df.empty:
        fig = go.Figure().add_annotation(text="No IPC data", x=0.5, y=0.5, showarrow=False)
        return template.apply(fig, plot_title="IPC Comparison", height=500)

    df_ipc = profiling_display_df[
        (profiling_display_df['Event Name'] == 'IPC') &
        (profiling_display_df['Statistic'] == 'Mean') 
    ].copy()

    if df_ipc.empty:
        fig = go.Figure().add_annotation(text="No IPC data points to plot", x=0.5, y=0.5, showarrow=False)
        return template.apply(fig, plot_title="IPC Comparison", height=500)

    fig = go.Figure()
    for (example, problem_size, version), group in df_ipc.groupby(['Example Name', 'Problem Size', 'Version']):
        group = group.sort_values('Threads')
        if group.empty or group['Value'].isnull().all(): continue
        current_color = PRIMARY_COLOR if version == 'CARTS' else ACCENT_COLOR
        current_dash = 'solid' if version == 'CARTS' else 'dash'
        fig.add_trace(go.Scatter(
            x=group['Threads'], y=group['Value'], mode='lines+markers', name=f"{example} (S:{problem_size}) - {version}",
            line=dict(color=current_color, width=2, dash=current_dash),
            marker=dict(size=8, color=current_color, line=dict(width=1, color=MARKER_OUTLINE_COLOR)),
            hovertemplate=f"<b>{example} ({version})</b><br>Size: {problem_size}<br>Threads: %{{x}}<br>IPC: %{{y:.3f}}<extra></extra>"
        ))
    fig.add_shape(type="line", x0=0,y0=1.0,x1=1,y1=1.0,line=dict(color="grey",width=1,dash="dash"),xref="paper",yref="y") 
    fig.update_layout(xaxis_type='category')
    return template.apply(fig, plot_title="Instructions Per Cycle (IPC) Comparison", xaxis_title="Threads",
                          yaxis_title="IPC", height=550, legend_title="Ex (Size) - Ver")

def create_stalls_analysis_plot(profiling_display_df, template):
    if profiling_display_df.empty:
        fig = go.Figure().add_annotation(text="No stalls data", x=0.5, y=0.5, showarrow=False)
        return template.apply(fig, plot_title="CPU Stalls Analysis", height=500), ""

    df_stalls_raw = profiling_display_df[
        (profiling_display_df['category'] == 'Stalls') & 
        (profiling_display_df['Statistic'] == 'Avg') 
    ].copy()

    if df_stalls_raw.empty:
        fig = go.Figure().add_annotation(text="No stall-related events found", x=0.5, y=0.5, showarrow=False)
        return template.apply(fig, plot_title="CPU Stalls Analysis", height=500), "- No stall-related events found."

    priority_stalls = ['cycle_activity.stalls_total:u', 'stalled-cycles-backend:u', 'stalled-cycles-frontend:u', 'resource_stalls.any:u', 'lock_contention:u']
    event_to_plot = None
    for p_event in priority_stalls:
        if p_event in df_stalls_raw['Event Name'].unique():
            event_to_plot = p_event
            break
    if not event_to_plot and not df_stalls_raw.empty: 
        event_to_plot = df_stalls_raw['Event Name'].unique()[0]

    if not event_to_plot: 
         fig = go.Figure().add_annotation(text="Could not determine stall event to plot.", x=0.5,y=0.5,showarrow=False)
         return template.apply(fig, plot_title="CPU Stalls Analysis", height=500), "- Could not determine stall event to plot."

    df_stalls_event = df_stalls_raw[df_stalls_raw['Event Name'] == event_to_plot].copy()
    if df_stalls_event.empty:
        fig = go.Figure().add_annotation(text=f"No data for event: {event_to_plot}", x=0.5,y=0.5,showarrow=False)
        return template.apply(fig, plot_title=f"CPU Stalls: {event_to_plot.split(':')[0]}", height=500), f"- No data found for event: {event_to_plot.split(':')[0]}"

    fig = go.Figure(); plot_as_percentage = False; specific_help = ""
    
    if 'cycle_activity' in event_to_plot:
        cycles_df = profiling_display_df[
            (profiling_display_df['Event Name'].str.contains('cycles:u', case=False)) & 
            (~profiling_display_df['Event Name'].str.contains('stall', case=False)) & 
            (profiling_display_df['Statistic'] == 'Avg')
        ]
        if not cycles_df.empty:
            df_stalls_event = pd.merge(
                df_stalls_event, 
                cycles_df[['Example Name', 'Problem Size', 'Threads', 'Version', 'Value']],
                on=['Example Name', 'Problem Size', 'Threads', 'Version'], 
                suffixes=('', '_cycles_total'), how='left'
            )
            if 'Value_cycles_total' in df_stalls_event.columns and df_stalls_event['Value_cycles_total'].notna().any():
                df_stalls_event['stall_percentage'] = np.where(
                    df_stalls_event['Value_cycles_total'] > 0,
                    (df_stalls_event['Value'] / df_stalls_event['Value_cycles_total']) * 100,
                    np.nan 
                )
                plot_as_percentage = True
                specific_help = f"- Shows data for `{event_to_plot.split(':')[0]}` as a percentage of total cycles (`cycles:u`)."

    y_col = 'stall_percentage' if plot_as_percentage else 'Value'
    y_title = f"{event_to_plot.split(':')[0]} (% of Cycles)" if plot_as_percentage else f"{event_to_plot.split(':')[0]} (Count, log)"
    y_type = 'linear' if plot_as_percentage else 'log' 
    if not specific_help: 
        specific_help = f"- Shows raw counts for event: `{event_to_plot.split(':')[0]}` (log scale)."

    for (ex, ps, ver), group in df_stalls_event.groupby(['Example Name', 'Problem Size', 'Version']):
        group = group.sort_values('Threads')
        if group.empty or y_col not in group.columns or group[y_col].isnull().all(): continue
        
        current_color = PRIMARY_COLOR if ver == 'CARTS' else ACCENT_COLOR
        current_dash = 'solid' if ver == 'CARTS' else 'dash'
        
        ht_val_format = ":.2f}%" if plot_as_percentage else ":,.0f"
        ht = f"<b>{ex} ({ver})</b><br>Size: {ps}<br>Threads: %{{x}}<br>"
        ht += f"{event_to_plot.split(':')[0]}: %{{y{ht_val_format}}}" 
        ht += "<extra></extra>"
        
        fig.add_trace(go.Scatter(
            x=group['Threads'], y=group[y_col], mode='lines+markers', name=f"{ex} (S:{ps}) - {ver}",
            line=dict(color=current_color, width=2, dash=current_dash),
            marker=dict(size=8, color=current_color, line=dict(width=1, color=MARKER_OUTLINE_COLOR)),
            hovertemplate=ht
        ))
    fig.update_layout(xaxis_type='category', yaxis_type=y_type)
    return template.apply(fig, plot_title=f"CPU Stalls: {event_to_plot.split(':')[0]}", xaxis_title="Threads",
                          yaxis_title=y_title, height=550, legend_title="Ex (Size) - Ver"), specific_help

# --- Helper function for plot wrappers ---
def create_plot_with_help(graph_component, help_text_md, plot_id_base):
    help_icon_id = {'type': 'help-icon-btn', 'index': plot_id_base}
    modal_id = {'type': 'help-modal', 'index': plot_id_base}
    close_button_id = {'type': 'close-help-modal-btn', 'index': plot_id_base}

    return html.Div([
        html.Div([
            graph_component,
            html.Button(
                "ⓘ",
                id=help_icon_id,
                n_clicks=0,
                className="plot-help-icon",
                style={
                    'fontSize': '1.2em', 'cursor': 'pointer', 'marginLeft': '10px',
                    'color': PRIMARY_COLOR, 'position': 'absolute', 'top': '10px', 'right': '10px',
                    'zIndex': '10', 'background': 'none', 'border': 'none', 'padding': '0'
                }
            )
        ], style={'position': 'relative', 'paddingTop': '30px'}),
        dbc.Modal(
            id=modal_id,
            is_open=False,
            children=[
                dbc.ModalHeader(dbc.ModalTitle(f"About: {plot_id_base.replace('-', ' ').title()}")),
                dbc.ModalBody(dcc.Markdown(help_text_md)),
                dbc.ModalFooter(
                    dbc.Button("Close", id=close_button_id, className="ms-auto", n_clicks=0, color="secondary")
                )
            ],
            centered=True,
            scrollable=True,
            size="lg" 
        )
    ])

# --- Dash App Layout Definition ---
app = dash.Dash(__name__, suppress_callback_exceptions=True,
                external_stylesheets=[dbc.themes.BOOTSTRAP, 'https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.3/css/all.min.css'])

graph_style = {'width': '100%', 'height': '550px'} 

app.layout = dbc.Container(fluid=True, style=StyleManager.MAIN_DIV_STYLE, children=[
    dcc.Store(id='full-results-store'), 
    dcc.Store(id='filters-store', data={'examples': [], 'threads': [], 'sizes': [], 'correctness': 'ALL'}),
    dcc.Store(id='filter-modal-store'), 
    html.Div(id='dummy-output-for-resize', style={'display': 'none'}),

    dbc.Row(dbc.Col(html.Div([
        html.H1([
            html.Span("CARTS", style={'color': PRIMARY_COLOR, 'fontWeight': '700'}),
            " Performance Dashboard"
        ], style=StyleManager.HEADER_STYLE),
        html.P("Benchmark Comparison between CARTS and OpenMP Implementations", style=StyleManager.SUBTITLE_STYLE),
    ]), width=12), className="mb-4 mt-3"),
    
    # File Upload Section
    dbc.Row([
        dbc.Col(html.Div([
            dbc.Row([
                dbc.Col(dcc.Upload(
                    id='upload-results-data',
                    children=html.Div([
                        html.I(className="fas fa-upload me-2"),
                        'Drag and Drop or ',
                        html.A('Select JSON Results File', className="fw-bold", style={'color': ACCENT_COLOR})
                    ]),
                    style=StyleManager.UPLOAD_STYLE,
                    multiple=False 
                ), md=8),
                dbc.Col(dbc.Checkbox(
                    id='use-default-path-checkbox',
                    label="Use Default Results Path",
                    value=True,
                    className="mt-2 pt-1"
                ), md=4, className="d-flex align-items-center")
            ]),
            html.Div(id='output-data-upload-status', className="text-center mt-2 small") 
        ], style=StyleManager.UPLOAD_CONTAINER_STYLE), width={"size": 8, "offset": 2})
    ], className="mb-4"),
    
    dbc.Row(dbc.Col(html.Div(id='data-load-status-message', className="text-center fw-bold mb-3", style={'color': ACCENT_COLOR}), width=12)),

    dbc.Row(dbc.Col(dbc.Button([html.I(className="fas fa-filter me-2"), "Filter Results"],
                    id='open-filter-modal-button', n_clicks=0, color="primary", size="lg", className="d-block mx-auto shadow-sm", style={'backgroundColor': ACCENT_COLOR, 'borderColor': ACCENT_COLOR}),
    width={"size": 6, "offset": 3}), className="mb-4"),

    dbc.Card(className="mb-4 shadow-sm", children=[
        dbc.CardHeader(html.H2("Benchmark Performance Analysis", className="mb-0", style={'color': PRIMARY_COLOR, 'fontSize': '1.5rem'})),
        dbc.CardBody([
            dcc.Tabs(id='benchmark-plot-tabs', value='scaling', className="custom-tabs", children=[
                dcc.Tab(label="Scaling", value='scaling', children=[html.Div(id='scaling-plot-div', className="mt-3")]),
                dcc.Tab(label="Parallel Efficiency", value='efficiency', children=[html.Div(id='efficiency-plot-div', className="mt-3")]),
                dcc.Tab(label="Speedup (OMP/CARTS)", value='speedup', children=[html.Div(id='speedup-plot-div', className="mt-3")]),
                dcc.Tab(label="Slowdown (CARTS/OMP)", value='slowdown', children=[html.Div(id='slowdown-plot-div', className="mt-3")]),
                dcc.Tab(label="Runtime Variability", value='variability', children=[
                    html.Div(id='variability-plot-area', children=[
                        dcc.Tabs(id='variability-sub-tabs-nav', value=None, children=[], className="custom-sub-tabs mt-2")
                    ], style={'display': 'none'}, className="mt-3") 
                ]),
            ]),
            html.Hr(className="my-4"),
            html.H3("Benchmark Summary Table", style={'color': PRIMARY_COLOR, 'fontSize': '1.3rem'}, className="mb-3"),
            dbc.Button([html.I(className="fas fa-download me-2"),"Download Benchmark CSV"], id="download-benchmark-csv-button", n_clicks=0, color="secondary", outline=True, size="sm", className="mb-3 shadow-sm"),
            dcc.Download(id="download-benchmark-data"),
            html.Div(id='benchmark-summary-table-div'),
        ])
    ]),

    html.Div(id='filter-modal-div'), 
    
    dbc.Card(className="mb-4 shadow-sm", children=[
        dbc.CardHeader(html.H2("Hardware Profiling Analysis", className="mb-0", style={'color': PRIMARY_COLOR, 'fontSize': '1.5rem'})),
        dbc.CardBody([
            dcc.Dropdown(id='profiling-category-dropdown', options=[], placeholder="Select Profiling Category...", clearable=False,
                         className="mb-3", style={'fontSize': '1rem'}),
            html.Div(id='profiling-plot-div', className="mt-3"), 
            html.Hr(className="my-4"),
            html.H3("Profiling Data Table", style={'color': PRIMARY_COLOR, 'fontSize': '1.3rem'}, className="mb-3"),
            dbc.Button([html.I(className="fas fa-download me-2"),"Download Profiling CSV"], id="download-profiling-csv-button", n_clicks=0, color="secondary", outline=True, size="sm", className="mb-3 shadow-sm"),
            dcc.Download(id="download-profiling-data"),
            html.Div(id='profiling-summary-table-div'),
        ])
    ]),
    
    dbc.Card(id='system-info-display-card', className="shadow-sm"),

    html.Footer(dbc.Row(dbc.Col(html.P([
        html.Span("CARTS Benchmarking Report | "),
        html.A("GitHub", href="https://github.com/ARTS-Lab/carts", target="_blank", style={'color': PRIMARY_COLOR, 'textDecoration': 'none', 'fontWeight': 'bold'}),
        html.Span(f" | © {datetime.datetime.now().year}")
    ], className="text-center text-muted small"), width=12)), className="mt-5 mb-3 pt-3 border-top")
])


# --- Callbacks ---
@app.callback(
    [Output('full-results-store', 'data'),
     Output('data-load-status-message', 'children'),
     Output('output-data-upload-status', 'children')],
    [Input('upload-results-data', 'contents'),
     Input('use-default-path-checkbox', 'value')],
    [State('upload-results-data', 'filename'),
     State('full-results-store', 'data')] 
)
def update_output_on_upload_or_default(uploaded_contents, use_default_path, 
                                       uploaded_filename, existing_data):
    ctx_triggered_id = ctx.triggered_id
    
    # Prioritize checkbox if it was the one that changed state to True
    if ctx_triggered_id == 'use-default-path-checkbox' and use_default_path:
        if os.path.exists(DEFAULT_RESULTS_PATH):
            full_data, error_msg = load_full_json_data(DEFAULT_RESULTS_PATH)
            if error_msg:
                status_msg = dbc.Alert(f"Error loading default file: {error_msg}", color="danger", dismissable=True)
                return None, status_msg, dash.no_update 
            if full_data and "detailed_aggregated_results" in full_data:
                status_msg = dbc.Alert(f"Successfully loaded default data from {os.path.basename(DEFAULT_RESULTS_PATH)}.", color="success", dismissable=True, duration=5000)
                return full_data, dash.no_update, status_msg
            # File exists but invalid
            else:
                status_msg = dbc.Alert("Default results file is invalid or empty.", color="danger", dismissable=True)
                return None, status_msg, dash.no_update
        # Default path does not exist
        else:
            status_msg = dbc.Alert(f"Default results file not found at: {DEFAULT_RESULTS_PATH}", color="warning", dismissable=True)
            return None, status_msg, dash.no_update

    # If checkbox is not checked, or it was not the trigger, try uploaded file
    if not use_default_path and uploaded_contents is not None:
        content_type, content_string = uploaded_contents.split(',')
        decoded = base64.b64decode(content_string)
        try:
            if uploaded_filename and 'json' in uploaded_filename.lower():
                data_str = decoded.decode('utf-8')
                full_data = json.loads(data_str)
                if "detailed_aggregated_results" not in full_data or not isinstance(full_data["detailed_aggregated_results"], list):
                    status_msg = dbc.Alert(f"Uploaded file '{uploaded_filename}' is missing key data or has incorrect format.", color="danger", dismissable=True)
                    return existing_data, dash.no_update, status_msg 
                status_msg = dbc.Alert(f"Successfully loaded data from: {uploaded_filename}", color="success", dismissable=True, duration=5000)
                return full_data, dash.no_update, status_msg 
            else:
                status_msg = dbc.Alert(f"File '{uploaded_filename}' is not a JSON file.", color="warning", dismissable=True)
                return existing_data, dash.no_update, status_msg
        except Exception as e:
            print(f"Error processing uploaded file: {e}")
            status_msg = dbc.Alert(f"Error processing file '{uploaded_filename}': {str(e)}", color="danger", dismissable=True)
            return existing_data, dash.no_update, status_msg
            
    # If checkbox is checked but it wasn't the trigger (e.g. initial load and checkbox is already True)
    if use_default_path and existing_data is None: # And no data loaded yet
         if os.path.exists(DEFAULT_RESULTS_PATH):
            full_data, error_msg = load_full_json_data(DEFAULT_RESULTS_PATH)
            if error_msg:
                status_msg = dbc.Alert(f"Error loading default file: {error_msg}", color="danger", dismissable=True)
                return None, status_msg, dash.no_update
            if full_data and "detailed_aggregated_results" in full_data:
                status_msg = dbc.Alert(f"Initial load: Using default data from {os.path.basename(DEFAULT_RESULTS_PATH)}.", color="info", dismissable=True, duration=5000)
                return full_data, dash.no_update, status_msg
            else:
                status_msg = dbc.Alert("Default results file is invalid or empty.", color="danger", dismissable=True)
                return None, status_msg, dash.no_update
         else:
            status_msg = dbc.Alert(f"Default results file not found at: {DEFAULT_RESULTS_PATH}. Please upload a file or ensure the default path is correct.", color="warning", dismissable=True)
            return None, status_msg, dash.no_update

    # Fallback: if no action taken (e.g. initial load, checkbox false, no upload) or if checkbox is unchecked and no file uploaded
    if existing_data is None and not use_default_path:
         return None, "Please upload a JSON results file or select 'Use Default Path'.", dash.no_update
    
    # If data already exists and no new action, keep existing data
    return existing_data, dash.no_update, dash.no_update


app.clientside_callback(
    """
    function(main_tab_value, variability_sub_tab_value, profiling_category_value) {
        // Debounce function
        let timeoutId;
        function debounceResize(graph_ids) {
            clearTimeout(timeoutId);
            timeoutId = setTimeout(() => {
                graph_ids.forEach(function(graph_id_obj) {
                    let graph_id = typeof graph_id_obj === 'string' ? graph_id_obj : graph_id_obj.id;
                    if (!graph_id) return;

                    var graphElement = document.getElementById(graph_id);
                    if (graphElement && graphElement.offsetParent !== null && typeof Plotly !== 'undefined') {
                        try {
                            Plotly.Plots.resize(graphElement);
                        } catch (e) {
                            // console.warn('Error resizing plot:', graph_id, e);
                        }
                    }
                });
            }, 150); 
        }

        var base_graph_ids = [
            'scaling-graph-id', 'efficiency-graph-id', 'speedup-graph-id', 
            'slowdown-graph-id', 'profiling-event-graph-id'
        ];
        
        var all_ids_to_check = [...base_graph_ids];

        if (main_tab_value === 'variability' && variability_sub_tab_value) {
            let sanitized_sub_tab_id = String(variability_sub_tab_value).replace(/[^a-zA-Z0-9_]/g, '_');
            if (sanitized_sub_tab_id) {
                 all_ids_to_check.push('variability-graph-' + sanitized_sub_tab_id);
            }
        }
        debounceResize(all_ids_to_check);
        return window.dash_clientside.no_update;
    }
    """,
    Output('dummy-output-for-resize', 'children'), 
    [Input('benchmark-plot-tabs', 'value'),
     Input('variability-sub-tabs-nav', 'value'), 
     Input('profiling-category-dropdown', 'value')]
)

@app.callback(
    [Output('scaling-plot-div', 'children'),
     Output('efficiency-plot-div', 'children'),
     Output('speedup-plot-div', 'children'),
     Output('slowdown-plot-div', 'children')],
    [Input('filters-store', 'data'),
     Input('full-results-store', 'data')]
)
def update_main_benchmark_graphs(filters, full_json_data):
    if not full_json_data:
        empty_fig = default_plot_template.apply(
            go.Figure().add_annotation(text="No data loaded.", xref="paper", yref="paper", showarrow=False))
        empty_plot_component = create_plot_with_help(
            dcc.Graph(figure=empty_fig, style=graph_style, id="empty-main-graph"), "Load data to view plots.", "empty-main")
        return [empty_plot_component] * 4

    data_manager = DataManager(full_json_data)
    plot_manager = PlotManager(default_plot_template, data_manager, plot_help_manager)

    scaling_fig = plot_manager.get_scaling_plot(filters)
    scaling_plot_component = create_plot_with_help(
        dcc.Graph(id='scaling-graph-id', figure=scaling_fig, style=graph_style), 
                  plot_help_manager.get_help_text("scaling"), "scaling")

    efficiency_fig = plot_manager.get_efficiency_plot(filters)
    efficiency_plot_component = create_plot_with_help(
        dcc.Graph(id='efficiency-graph-id', figure=efficiency_fig, style=graph_style), 
                  plot_help_manager.get_help_text("efficiency"), "efficiency")

    speedup_fig = plot_manager.get_speedup_plot(filters)
    speedup_plot_component = create_plot_with_help(
        dcc.Graph(id='speedup-graph-id', figure=speedup_fig, style=graph_style), 
                  plot_help_manager.get_help_text("speedup"), "speedup")
    
    slowdown_fig = plot_manager.get_slowdown_plot(filters)
    slowdown_plot_component = create_plot_with_help(
        dcc.Graph(id='slowdown-graph-id', figure=slowdown_fig, style=graph_style), 
                  plot_help_manager.get_help_text("slowdown"), "slowdown")
    return scaling_plot_component, efficiency_plot_component, speedup_plot_component, slowdown_plot_component

@app.callback(
    [Output('variability-sub-tabs-nav', 'children'),
     Output('variability-sub-tabs-nav', 'value'),
     Output('variability-plot-area', 'style')], 
    [Input('filters-store', 'data'), 
     Input('full-results-store', 'data'),
     Input('benchmark-plot-tabs', 'value')] 
)
def update_variability_plots_area(filters, full_json_data, active_main_tab):
    if active_main_tab != 'variability' or not full_json_data:
        return [], None, {'display': 'none'} 

    data_manager = DataManager(full_json_data)
    plot_manager = PlotManager(default_plot_template, data_manager, plot_help_manager)
    sub_tabs_children, default_active_sub_tab = plot_manager.get_variability_plot_components(filters)
    
    if not sub_tabs_children: 
         return [
            html.Div("No variability data to display after filtering.", 
                     style={'padding':'10px', 'textAlign':'center', 'color': WARNING_COLOR})], None, {'display': 'block'}

    return sub_tabs_children, default_active_sub_tab, {'display': 'block'}

@app.callback(
    Output('benchmark-summary-table-div', 'children'),
    [Input('filters-store', 'data'),
     Input('full-results-store', 'data')]
)
def update_benchmark_summary_table_display(filters, full_json_data):
    if not full_json_data:
        return html.Div("No data loaded to display table.", style={'color': ACCENT_COLOR, 'fontWeight': 'bold', 'padding': '12px'})

    data_manager = DataManager(full_json_data)
    table_manager = TableManager(font_family=default_plot_template.font_family)
    
    benchmark_data_for_table = data_manager.get_benchmark_display_data(filters)
    return table_manager.create_benchmark_summary_table(benchmark_data_for_table)

@app.callback(
    Output('profiling-plot-div', 'children'),
    [Input('profiling-category-dropdown', 'value'),
     Input('filters-store', 'data'),
     Input('full-results-store', 'data')]
)
def update_profiling_event_plot(selected_category, filters, full_json_data):
    if not full_json_data:
        empty_fig = default_plot_template.apply(
            go.Figure().add_annotation(text="Load data to see profiling analysis.", xref="paper", yref="paper", showarrow=False))
        return create_plot_with_help(
            dcc.Graph(id='profiling-event-graph-id', figure=empty_fig, style=graph_style), "No profiling data loaded.", "prof-empty")

    if not selected_category:
        empty_fig = default_plot_template.apply(
            go.Figure().add_annotation(text="Select a profiling category.", xref="paper", yref="paper", showarrow=False))
        return create_plot_with_help(
            dcc.Graph(id='profiling-event-graph-id', figure=empty_fig, style=graph_style), "Please select a category.", "prof-select")

    data_manager = DataManager(full_json_data)
    plot_manager = PlotManager(default_plot_template, data_manager, plot_help_manager)
    
    fig = go.Figure()
    specific_event_help_info = "" 

    if selected_category == 'Cache':
        fig, specific_event_help_info = plot_manager.get_cache_plot(filters)
        help_text = plot_help_manager.get_help_text("cache_perf", specific_event_help_info)
    elif selected_category == 'CPU':
        fig = plot_manager.get_ipc_plot(filters) 
        help_text = plot_help_manager.get_help_text("ipc_perf")
    elif selected_category == 'Stalls':
        fig, specific_event_help_info = plot_manager.get_stalls_plot(filters)
        help_text = plot_help_manager.get_help_text("stalls_perf", specific_event_help_info)
    else: 
        fig, specific_event_help_info = plot_manager.get_generic_profiling_plot(selected_category, filters)
        help_text = plot_help_manager.get_help_text("generic_profiling", specific_event_help_info)
    
    return create_plot_with_help(dcc.Graph(id='profiling-event-graph-id', figure=fig, style=graph_style), help_text, f"prof-{selected_category.lower().replace(' ', '-')}")

@app.callback(
    Output('profiling-summary-table-div', 'children'),
    [Input('filters-store', 'data'), Input('full-results-store', 'data')]
)
def update_profiling_summary_table_display(filters, full_json_data):
    if not full_json_data:
        return html.Div("Load data to see profiling summary table.", style={'padding':'10px', 'textAlign':'center'})

    data_manager = DataManager(full_json_data)
    table_manager = TableManager(font_family=default_plot_template.font_family)
    
    profiling_data_for_table = data_manager.get_profiling_display_data(filters)
    return table_manager.create_profiling_summary_table(profiling_data_for_table)

@app.callback(
    Output('profiling-category-dropdown', 'options'),
    [Input('full-results-store', 'data')]
)
def update_profiling_category_dropdown_options(full_json_data):
    if not full_json_data: return []
    
    data_manager = DataManager(full_json_data)
    profiling_df_for_categories = data_manager.get_profiling_display_data() 
    
    if profiling_df_for_categories.empty or 'category' not in profiling_df_for_categories.columns:
        return []
    
    categories = sorted(profiling_df_for_categories['category'].unique())
    if 'Other' in categories:
        categories.remove('Other')
        return [{'label': cat, 'value': cat} for cat in categories] + [{'label': 'Other', 'value': 'Other'}]
    return [{'label': cat, 'value': cat} for cat in categories]

# --- Filter Modal Callbacks & Rendering ---
def render_filter_modal_layout(modal_state, filter_options):
    ex_opts = filter_options.get('examples', [])
    th_opts = filter_options.get('threads', [])
    sz_opts = filter_options.get('sizes', [])
    corr_opts = filter_options.get('correctness', [])

    ex_val = modal_state.get('examples', [opt['value'] for opt in ex_opts])
    th_val = modal_state.get('threads', [opt['value'] for opt in th_opts])
    sz_val = modal_state.get('sizes', [opt['value'] for opt in sz_opts])
    corr_val = modal_state.get('correctness', 'ALL')

    select_all_style = {'color': ACCENT_COLOR, 'fontWeight': 'bold', 'cursor': 'pointer', 'fontSize': '0.9em', 'marginLeft': '10px', 'textDecoration': 'underline'}
    checklist_card_style = {
        'background': '#f8f9fa', 'borderRadius': '6px', 
        'padding': '10px', 'marginBottom': '10px', 'width': '100%', 'maxHeight': '120px', 'overflowY': 'auto', 
        'border': f'1px solid {GRID_COLOR}', 'fontSize': '0.95rem',
    }
    accent_bar_style = {
        'height': '5px', 'width': '100%', 'background': f'linear-gradient(90deg, {PRIMARY_COLOR} 0%, {ACCENT_COLOR} 100%)',
        'borderTopLeftRadius': '12px', 'borderTopRightRadius': '12px', 'marginBottom': '15px',
    }
    section_header_style = {'fontWeight': '600', 'fontSize': '1rem', 'color': PRIMARY_COLOR, 'marginBottom': '4px'}

    if not ex_opts and not th_opts and not sz_opts:
        return dbc.Modal([
            dbc.ModalHeader(dbc.ModalTitle("Filter Results")),
            dbc.ModalBody("No data loaded or no filterable options available.")
        ], id="filter-modal-actual", is_open=True, centered=True)

    modal_content = dbc.Container(fluid=True, children=[
        html.Div(style=accent_bar_style),
        dbc.Row([
            dbc.Col(html.H3('Filter Results', className="mb-3", style={'color': PRIMARY_COLOR, 'textAlign': 'center', 'fontSize': '1.5rem', 'fontWeight': '600'}), width=11),
            dbc.Col(
                html.Button('×', id={'type': 'close-filter-modal-button-action', 'index': 0}, className="btn-close", 
                            style=StyleManager.FILTER_MODAL_CLOSE_STYLE), width=1, className="text-end")
        ], className="align-items-center mb-3"),

        dbc.Row([
            dbc.Col([
                html.Div([
                    html.Label('Examples:', style=section_header_style),
                    html.Span([
                        html.A('Select All', id='select-all-examples-filter', n_clicks=0, href="#", style=select_all_style),
                        " / ",
                        html.A('Clear All', id='clear-all-examples-filter', n_clicks=0, href="#", style=select_all_style),
                    ], className="float-end")
                ], className="clearfix mb-1"),
                dcc.Checklist(id='modal-filter-example-checklist', options=ex_opts, value=ex_val, style=checklist_card_style, inputClassName="me-1"),
            ], md=6),
            dbc.Col([
                html.Div([
                    html.Label('Threads:', style=section_header_style),
                     html.Span([
                        html.A('Select All', id='select-all-threads-filter', n_clicks=0, href="#", style=select_all_style),
                        " / ",
                        html.A('Clear All', id='clear-all-threads-filter', n_clicks=0, href="#", style=select_all_style),
                    ], className="float-end")
                ], className="clearfix mb-1"),
                dcc.Checklist(id='modal-filter-thread-checklist', options=th_opts, value=th_val, style=checklist_card_style, inputClassName="me-1"),
            ], md=6)
        ], className="mb-2"),

        dbc.Row([
            dbc.Col([
                html.Div([
                    html.Label('Problem Sizes:', style=section_header_style),
                    html.Span([
                        html.A('Select All', id='select-all-sizes-filter', n_clicks=0, href="#", style=select_all_style),
                        " / ",
                        html.A('Clear All', id='clear-all-sizes-filter', n_clicks=0, href="#", style=select_all_style),
                    ], className="float-end")
                ], className="clearfix mb-1"),
                dcc.Checklist(id='modal-filter-size-checklist', options=sz_opts, value=sz_val, style=checklist_card_style, inputClassName="me-1"),
            ], md=6),
            dbc.Col([
                html.Label('Correctness Status:', style=section_header_style, className="d-block mb-1"),
                dcc.Dropdown(id='modal-filter-correctness-dropdown', options=corr_opts, value=corr_val, clearable=False, style={'width': '100%', 'fontSize': '0.95rem'})
            ], md=6, className="mt-md-0 mt-3") 
        ], className="mb-3"),
        
        dbc.Row(dbc.Col([
            dbc.Button('Reset to Defaults', id={'type': 'reset-filters-button-action', 'index': 0}, color="secondary", outline=True, className="me-2 shadow-sm"),
            dbc.Button('Apply Filters', id={'type': 'apply-filters-button-action', 'index': 0}, color="primary", className="shadow-sm", style={'backgroundColor': ACCENT_COLOR, 'borderColor': ACCENT_COLOR}),
        ], width=12, className="text-center mt-3 pt-3 border-top"),
        )
    ])
    
    return dbc.Modal([
        dbc.ModalBody(modal_content)
    ], 
    id="filter-modal-actual", 
    is_open=True, 
    centered=True, 
    size="lg", 
    scrollable=True,
    backdrop="static", 
    keyboard=False, 
    style={'zIndex': 1055} 
    )

@app.callback( Output('modal-filter-example-checklist', 'value'),
    [Input('select-all-examples-filter', 'n_clicks'), Input('clear-all-examples-filter', 'n_clicks')],
    [State('modal-filter-example-checklist', 'options')], prevent_initial_call=True )
def toggle_modal_examples_select(select_all_n, clear_all_n, options):
    triggered_id = callback_context.triggered_id
    if not options: raise PreventUpdate
    if triggered_id == 'select-all-examples-filter': return [opt['value'] for opt in options]
    if triggered_id == 'clear-all-examples-filter': return []
    raise PreventUpdate

@app.callback( Output('modal-filter-thread-checklist', 'value'),
    [Input('select-all-threads-filter', 'n_clicks'), Input('clear-all-threads-filter', 'n_clicks')],
    [State('modal-filter-thread-checklist', 'options')], prevent_initial_call=True )
def toggle_modal_threads_select(select_all_n, clear_all_n, options):
    triggered_id = callback_context.triggered_id
    if not options: raise PreventUpdate
    if triggered_id == 'select-all-threads-filter': return [opt['value'] for opt in options]
    if triggered_id == 'clear-all-threads-filter': return []
    raise PreventUpdate

@app.callback( Output('modal-filter-size-checklist', 'value'),
    [Input('select-all-sizes-filter', 'n_clicks'), Input('clear-all-sizes-filter', 'n_clicks')],
    [State('modal-filter-size-checklist', 'options')], prevent_initial_call=True)
def toggle_modal_sizes_select(select_all, clear_all, options):
    triggered_id = callback_context.triggered_id
    if not options: raise PreventUpdate
    if triggered_id == 'select-all-sizes-filter': return [opt['value'] for opt in options]
    if triggered_id == 'clear-all-sizes-filter': return []
    raise PreventUpdate

@app.callback(
    [Output('filter-modal-div', 'children'), 
     Output('filter-modal-store', 'data', allow_duplicate=True)], 
    [Input('open-filter-modal-button', 'n_clicks'),
     Input({'type': 'close-filter-modal-button-action', 'index': ALL}, 'n_clicks'),
     Input({'type': 'reset-filters-button-action', 'index': ALL}, 'n_clicks')],
    [State('filters-store', 'data'),      
     State('full-results-store', 'data'), 
     State('filter-modal-store', 'data')], 
    prevent_initial_call=True
)
def manage_filter_modal_visibility_and_state(open_n, close_n_list, reset_n_list, 
                                             active_dashboard_filters, full_json_data, 
                                             current_modal_selections_state):
    triggered_id_obj = callback_context.triggered[0]
    triggered_id_str = triggered_id_obj['prop_id'].split('.')[0]
    
    action_type = None
    if triggered_id_str == 'open-filter-modal-button':
        action_type = 'open'
    elif triggered_id_str.startswith('{'): 
        try:
            id_dict = json.loads(triggered_id_str.replace("'", "\""))
            action_type = id_dict.get('type') 
        except:
            pass 

    if not action_type: raise PreventUpdate

    dm = DataManager(full_json_data)
    available_options = dm.get_available_filter_options()
    
    default_selections = { 
        'examples': [opt['value'] for opt in available_options.get('examples', [])],
        'threads': [opt['value'] for opt in available_options.get('threads', [])],
        'sizes': [opt['value'] for opt in available_options.get('sizes', [])],
        'correctness': 'ALL'
    }

    if action_type == 'open':
        selections_to_render = default_selections.copy()
        source_for_selections = active_dashboard_filters
        # Corrected condition: Iterate over items() to have both key (k_iter) and value (v_iter)
        if not (source_for_selections and any(
            v_iter is not None and (v_iter != [] if isinstance(v_iter, list) else True)
            for k_iter, v_iter in source_for_selections.items() 
            if k_iter != 'correctness' or v_iter != 'ALL' 
        )):
            source_for_selections = current_modal_selections_state

        if source_for_selections and any(
            v_iter is not None and (v_iter != [] if isinstance(v_iter, list) else True)
            for k_iter, v_iter in source_for_selections.items()
            if k_iter != 'correctness' or v_iter != 'ALL'
        ):
            for key_filter in default_selections:
                if key_filter in source_for_selections and source_for_selections[key_filter] is not None:
                    if isinstance(source_for_selections[key_filter], list) or source_for_selections[key_filter] == 'ALL':
                         selections_to_render[key_filter] = source_for_selections[key_filter]
        
        return render_filter_modal_layout(selections_to_render, available_options), selections_to_render

    elif action_type == 'close-filter-modal-button-action':
        return None, current_modal_selections_state 

    elif action_type == 'reset-filters-button-action':
        return render_filter_modal_layout(default_selections, available_options), default_selections

    return dash.no_update, dash.no_update

@app.callback(
    [Output('filters-store', 'data'), 
     Output('filter-modal-store', 'data', allow_duplicate=True), 
     Output('filter-modal-div', 'children', allow_duplicate=True)], 
    [Input({'type': 'apply-filters-button-action', 'index': ALL}, 'n_clicks')],
    [State('modal-filter-example-checklist', 'value'), 
     State('modal-filter-thread-checklist', 'value'),
     State('modal-filter-size-checklist', 'value'),
     State('modal-filter-correctness-dropdown', 'value')],
    prevent_initial_call=True
)
def apply_modal_filters_to_dashboard(apply_n_list, modal_ex_val, modal_th_val, modal_sz_val, modal_corr_val):
    if not any(n for n in apply_n_list if n is not None): 
        raise PreventUpdate
    
    new_dashboard_filters = {
        'examples': modal_ex_val if modal_ex_val is not None else [],
        'threads': modal_th_val if modal_th_val is not None else [],
        'sizes': modal_sz_val if modal_sz_val is not None else [],
        'correctness': modal_corr_val if modal_corr_val is not None else 'ALL'
    }
    return new_dashboard_filters, new_dashboard_filters, None 

@app.callback(
    Output('system-info-display-card', 'children'),
    [Input('full-results-store', 'data')]
)
def update_system_info_display(full_json_data):
    if not full_json_data: return None
    
    data_manager = DataManager(full_json_data)
    sysinfo = data_manager.get_system_info()
    
    if not sysinfo or not isinstance(sysinfo, dict):
        return dbc.CardBody(html.P("System information not available.", className="text-muted"))
    
    items = []
    sys_map = {
        'timestamp': ('Timestamp', None),
        'cpu_model': ('CPU Model', None), 
        'cpu_cores': ('CPU Cores', 0),
        'memory_total_mb': ('Total Memory', 0), 
        'os_platform': ('OS Platform', None),
        'clang_version': ('Clang Version', None),
    }
    for key, (label, ndigits) in sys_map.items():
        if key in sysinfo and sysinfo[key] is not None:
            value = sysinfo[key]
            if key == 'memory_total_mb' and isinstance(value, (int, float)): 
                value_str = f"{format_number(value, 0)} MB"
            elif ndigits is not None:
                value_str = format_number(value, ndigits)
            else:
                value_str = str(value)
            items.append(html.Div([html.Strong(f"{label}: "), html.Span(value_str)], className="mb-1"))
    
    if not items: return dbc.CardBody(html.P("No detailed system information found.", className="text-muted"))
    
    return [
        dbc.CardHeader(html.H3("System Information", className="mb-0", style={'fontSize': '1.3rem'})),
        dbc.CardBody(items)
    ]

# --- Download CSV Callbacks ---
@app.callback(
    Output("download-benchmark-data", "data"), 
    Input("download-benchmark-csv-button", "n_clicks"),
    [State('filters-store', 'data'), State('full-results-store', 'data')], 
    prevent_initial_call=True
)
def download_benchmark_summary_csv(n_clicks, filters, full_json_data):
    if not full_json_data: raise PreventUpdate
    
    data_manager = DataManager(full_json_data)
    benchmark_df = data_manager.get_benchmark_display_data(filters) 
    
    if benchmark_df.empty: 
        print("No benchmark data to download after filtering.")
        raise PreventUpdate
        
    return dcc.send_data_frame(benchmark_df.to_csv, "benchmark_summary_report.csv", index=False)

@app.callback(
    Output("download-profiling-data", "data"), 
    Input("download-profiling-csv-button", "n_clicks"),
    [State('filters-store', 'data'), State('full-results-store', 'data')], 
    prevent_initial_call=True
)
def download_profiling_summary_csv(n_clicks, filters, full_json_data):
    if not full_json_data: raise PreventUpdate
        
    data_manager = DataManager(full_json_data)
    profiling_df = data_manager.get_profiling_display_data(filters) 
    
    if profiling_df.empty: 
        print("No profiling data to download after filtering.")
        raise PreventUpdate
        
    return dcc.send_data_frame(profiling_df.to_csv, "profiling_summary_report.csv", index=False)

# --- Help Modal Callbacks ---
@app.callback(
    Output({'type': 'help-modal', 'index': MATCH}, 'is_open'),
    [Input({'type': 'help-icon-btn', 'index': MATCH}, 'n_clicks'),
     Input({'type': 'close-help-modal-btn', 'index': MATCH}, 'n_clicks')],
    [State({'type': 'help-modal', 'index': MATCH}, 'is_open')],
    prevent_initial_call=True
)
def toggle_plot_help_modal(open_clicks, close_clicks, is_open):
    triggered_id = callback_context.triggered_id
    if not triggered_id: raise PreventUpdate 
    
    if open_clicks is None and close_clicks is None and not is_open: 
        raise PreventUpdate
    if (open_clicks and open_clicks > 0) or (close_clicks and close_clicks > 0) :
        return not is_open
    return is_open 

if __name__ == "__main__":
    app.run(debug=True)
