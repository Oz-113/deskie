import os

# Configuration
folder_path = "source_frames"  # Folder containing your EZGIF frames
prefix = "zerotwo_"             # Desired base name

# Gather and sort all PNG files to maintain the correct animation sequence
files = sorted([f for f in os.listdir(folder_path) if f.lower().endswith('.png')])

for index, filename in enumerate(files):
    old_path = os.path.join(folder_path, filename)
    file_extension = os.path.splitext(filename)[1].lower()
    new_filename = f"{prefix}{index}{file_extension}"
    new_path = os.path.join(folder_path, new_filename)
    
    os.rename(old_path, new_path)
    print(f"Renamed: {filename} -> {new_filename}")

print("All frames renamed successfully!")