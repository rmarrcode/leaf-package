import torch
import torch.nn as nn
import torch.optim as optim
import torchvision
import torchvision.transforms as transforms
from torch.utils.data import DataLoader
import time
import wandb
from leaf_trainer import leaf, create_leaf_config, save_config, get_optimal_batch_size

# Your existing model definition
class ResNetModel(nn.Module):
    def __init__(self):
        super().__init__()
        # Use ResNet-50 but modify first layer for CIFAR-10
        self.model = torchvision.models.resnet50(pretrained=True)
        self.model.conv1 = nn.Conv2d(3, 64, kernel_size=3, stride=1, padding=1, bias=False)
        self.model.maxpool = nn.Identity()  # Remove maxpool since CIFAR-10 images are small
        self.model.fc = nn.Linear(2048, 10)  # CIFAR-10 has 10 classes
    
    def forward(self, x):
        return self.model(x)

# Your existing training function, now with the @leaf decorator
def train(model, dataloader, criterion, optimizer, num_epochs=10):
    """Training function that will be distributed across available resources."""
    for epoch in range(num_epochs):
        model.train()
        running_loss = 0.0
        for i, (inputs, labels) in enumerate(dataloader):
            optimizer.zero_grad()
            outputs = model(inputs)
            loss = criterion(outputs, labels)
            loss.backward()
            optimizer.step()
            
            running_loss += loss.item()
            if i % 100 == 99:
                print(f'[{epoch + 1}, {i + 1:5d}] loss: {running_loss / 100:.3f}')
                running_loss = 0.0

# Main training setup
def main():
    # Auto-discover available resources
    config = create_leaf_config(auto_discover=True)
    
    # Save configuration for reference
    save_config(config, 'leaf_config.json')
    
    # Get optimal batch size based on available resources
    batch_size = get_optimal_batch_size(config)
    print(f"Using batch size: {batch_size}")
    
    # Set device
    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    
    # Hyperparameters
    num_epochs = 50
    learning_rate = 0.001
    
    # Data preprocessing
    transform_train = transforms.Compose([
        transforms.RandomCrop(32, padding=4),
        transforms.RandomHorizontalFlip(),
        transforms.ToTensor(),
        transforms.Normalize((0.4914, 0.4822, 0.4465), (0.2023, 0.1994, 0.2010)),
    ])
    
    # Load dataset
    trainset = torchvision.datasets.CIFAR10(root='./data', train=True,
                                          download=True, transform=transform_train)
    trainloader = DataLoader(trainset, batch_size=batch_size,
                           shuffle=True, num_workers=2)
    
    # Create model and training components
    model = ResNetModel().to(device)
    criterion = nn.CrossEntropyLoss()
    optimizer = optim.Adam(model.parameters(), lr=learning_rate)
    
    # Initialize wandb
    wandb.init(project="leaf-training")
    
    # Apply leaf decorator with discovered configuration
    train_with_leaf = leaf(config=config)(train)
    
    # Training loop
    print('Starting training...')
    for epoch in range(num_epochs):
        start_time = time.time()
        
        train_loss, train_acc = train_with_leaf(
            model=model,
            dataloader=trainloader,
            criterion=criterion,
            optimizer=optimizer
        )
        
        print(f'\nEpoch: {epoch + 1}/{num_epochs}')
        print(f'Time: {time.time() - start_time:.2f}s')
        print(f'Train Loss: {train_loss:.3f} | Train Acc: {train_acc:.2f}%')
        
        # Log metrics
        wandb.log({
            "epoch": epoch,
            "train_loss": train_loss,
            "train_acc": train_acc
        })

if __name__ == "__main__":
    main() 